#include "resource_monitor.h"
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <iostream>
#include <spdlog/spdlog.h>
#include <optional>
#include <map>

struct ResourceMonitor::Impl {
    std::optional<uint64_t> prevTotal_;
    std::optional<uint64_t> prevIdleTime_;

    std::map<std::string, uint64_t> diskIoTime_;
    std::optional<std::chrono::system_clock::time_point> diskIoUpdateTime_;
};

ResourceMonitor::ResourceMonitor()
    : impl_(new Impl) {
}

ResourceMonitor::~ResourceMonitor() {    
}

std::string ResourceMonitor::getCpuUsage() {
    std::ifstream proc_stat("/proc/stat");
    std::string line;
    std::getline(proc_stat, line);
    proc_stat.close();
    
    std::istringstream iss(line);
    std::string cpu;
    uint64_t user, nice, system, idle, iowait, irq, softirq;
    iss >> cpu >> user >> nice >> system >> idle >> iowait >> irq >> softirq;
    
    uint64_t total = user + nice + system + idle + iowait + irq + softirq;
    uint64_t idleTime = idle + iowait;
    
    if (!impl_->prevTotal_.has_value()) {
        impl_->prevTotal_ = total;
        impl_->prevIdleTime_ = idleTime;
        return "CPU: ?";  // 第一次调用无法计算使用率
    }
    
    uint64_t deltaTotal = total - impl_->prevTotal_.value();
    uint64_t deltaIdle = idleTime - impl_->prevIdleTime_.value();
    
    impl_->prevTotal_ = total;
    impl_->prevIdleTime_ = idleTime;
    
    if (deltaTotal == 0) return "CPU: ?";  // 避免除以零
    
    double cpu_usage = 100.0 * (deltaTotal - deltaIdle) / deltaTotal;
    return fmt::format("CPU: {:.2f}%", cpu_usage);
}

std::string ResourceMonitor::getMemoryUsage() {
    std::ifstream proc_meminfo("/proc/meminfo");
    if (!proc_meminfo) return "Mem: ?";

    uint64_t total = 0, free = 0, buffers = 0, cached = 0;
    uint64_t swapTotal = 0, swapFree = 0;
    std::string line;

    auto parseValue = [](const std::string& unit, uint64_t value) -> uint64_t {
        if (unit == "kB") return value * 1024;
        if (unit == "MB") return value * 1024 * 1024;
        if (unit == "GB") return value * 1024 * 1024 * 1024;
        if (unit == "TB") return value * 1024 * 1024 * 1024 * 1024;
        return value;  // 默认按字节处理
    };

    auto valueToHumanReadable = [](uint64_t value) -> std::string {
        if (value >= (uint64_t)1024 * 1024 * 1024 * 1024) {
            return fmt::format("{:.2f} TB", value / 1024.0 / 1024.0 / 1024.0 / 1024.0);
        }
        else if (value >= (uint64_t)1024 * 1024 * 1024) {
            return fmt::format("{:.2f} GB", value / 1024.0 / 1024.0 / 1024.0);
        }
        else if (value >= (uint64_t)1024 * 1024) {
            return fmt::format("{:.2f} MB", value / 1024.0 / 1024.0);
        }
        else if (value >= (uint64_t)1024) {
            return fmt::format("{:.2f} kB", value / 1024.0);
        }
        else {
            return fmt::format("{}B", value);
        }
    };

    while (std::getline(proc_meminfo, line)) {
        std::istringstream iss(line);
        std::string key, unit;
        uint64_t value;
        
        iss >> key >> value >> unit;
        
        if (key == "MemTotal:") {
            total = parseValue(unit, value);
        } else if (key == "MemFree:") {
            free = parseValue(unit, value);
        } else if (key == "Buffers:") {
            buffers = parseValue(unit, value);
        } else if (key == "Cached:") {
            cached = parseValue(unit, value);
        } else if (key == "SwapTotal:") {
            swapTotal = parseValue(unit, value);
        } else if (key == "SwapFree:") {
            swapFree = parseValue(unit, value);
        }
    }

    if (total == 0) return "Mem: ?";
    
    // 计算物理内存使用率
    uint64_t used = total - free - buffers - cached;
    double memoryUsage = 100.0 * used / total;

    std::string memoryUsageString = fmt::format("Mem: {:.2f}% ({} of {})",
        memoryUsage,
        valueToHumanReadable(used),
        valueToHumanReadable(total));
   
    // 如果有交换分区，计算交换分区使用率
    if (swapTotal > 0) {
        uint64_t swapUsed = swapTotal - swapFree;
        double swapUsage = 100.0 * swapUsed / swapTotal;
        return fmt::format("{}, Swap: {:.2f}% ({} of {})",
            memoryUsageString,
            swapUsage,
            valueToHumanReadable(swapUsed),
            valueToHumanReadable(swapTotal));
    }
    else {
        return memoryUsageString;
    }
}

std::string ResourceMonitor::getDiskUsage() {
    std::ifstream proc_diskstats("/proc/diskstats");
    if (!proc_diskstats) return "Disk: ?";

    std::string line;
    std::map<std::string, uint64_t> diskIoTime;

    while (std::getline(proc_diskstats, line)) {
        std::istringstream iss(line);
        unsigned major, minor;
        std::string diskName;
        uint64_t reads, readMerges, readSectors, readTicks;
        uint64_t writes, writeMerges, writeSectors, writeTicks;
        uint64_t inFlight, ioTimeMs, weightedIoTimeMs;

        iss >> major >> minor >> diskName
            >> reads >> readMerges >> readSectors >> readTicks
            >> writes >> writeMerges >> writeSectors >> writeTicks
            >> inFlight >> ioTimeMs >> weightedIoTimeMs;

        // 只统计物理磁盘(sdX, mmcblkX, nvmeXnY)和SD卡
        if (diskName.find("sd") == 0       // SATA/SCSI磁盘
            || diskName.find("hd") == 0       // SATA/SCSI磁盘
            || diskName.find("nvme") == 0     // NVMe SSD
            || diskName.find("mmcblk") == 0) {   // SD卡/eMMC
            diskIoTime.emplace(diskName, ioTimeMs);
        }
    }

    if(!impl_->diskIoUpdateTime_.has_value()) {
        impl_->diskIoUpdateTime_ = std::chrono::system_clock::now();
        impl_->diskIoTime_ = diskIoTime;
        return "Disk: ?";
    }

    // 计算磁盘繁忙百分比
    auto now = std::chrono::system_clock::now();
    auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - impl_->diskIoUpdateTime_.value()).count();

    std::string result;
    for(const auto& disk : diskIoTime) {
        const auto& diskName = disk.first;
        const auto& ioTimeMs = disk.second;

        auto prevIoTime = impl_->diskIoTime_.find(diskName);
        if(prevIoTime != impl_->diskIoTime_.end()) {
            auto prevIoTimeMs = prevIoTime->second;
            auto ioTimeDeltaMs = ioTimeMs - prevIoTimeMs;
            auto diskUsage = 100.0 * ioTimeDeltaMs / elapsedMs;
            if(!result.empty()) result += ", ";  // 不是第一个磁盘，添加逗号以分隔多磁盘情况
            result += fmt::format("Disk {}: {:.2f}%", diskName, diskUsage);
        }
    }

    impl_->diskIoUpdateTime_ = now;
    impl_->diskIoTime_ = diskIoTime;

    return result;
}