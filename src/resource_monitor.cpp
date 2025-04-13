#include "resource_monitor.h"
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <iostream>
#include <spdlog/spdlog.h>
#include <optional>
#include <map>
#include <istream>
#include <iterator>
#include <filesystem>

struct OnScopeExit {
    std::function<void()> func_;
    OnScopeExit(std::function<void()> func) : func_(func) {}
    ~OnScopeExit() { func_(); }
};

std::string valueToHumanReadable(uint64_t value) {
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
}

std::string getCmdLine(int pid) {
    std::string cmdlinePath = fmt::format("/proc/{}/cmdline", pid);
    std::ifstream cmdlineFile(cmdlinePath);
    if (!cmdlineFile.is_open()) return "";
    std::string cmdline;
    std::getline(cmdlineFile, cmdline, '\0');
    return cmdline;
}

struct ResourceMonitor::Impl {
    // CPU
    std::optional<uint64_t> prevTotal_;
    std::optional<uint64_t> prevIdleTime_;

    // 磁盘
    std::map<std::string, uint64_t> diskIoTime_;
    std::optional<std::chrono::system_clock::time_point> diskIoUpdateTime_;

    // 进程CPU占用
    struct ProcessTime {
        int pid_;
        uint64_t totalTime_;
    };
    std::optional<uint64_t> prevCpuTime_;   // 和prevTotal_略微相同  
    std::map<int, ProcessTime> processTimes_;

    // 进程IO占用
    struct ProcessIo {
        int pid_;
        uint64_t readIo_;
        uint64_t writeIo_;
    };
    std::map<int, ProcessIo> processIos_;

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
    
    OnScopeExit onScopeExit([&]() {
        impl_->prevTotal_ = total;
        impl_->prevIdleTime_ = std::move(idleTime);
    });

    if (!impl_->prevTotal_.has_value()) {
        return "CPU: ?";  // 第一次调用无法计算使用率
    }
    
    uint64_t deltaTotal = total - impl_->prevTotal_.value();
    uint64_t deltaIdle = idleTime - impl_->prevIdleTime_.value();
       
    if (deltaTotal == 0) return "CPU: ?";  // 避免除以零
    
    double cpuUsage = 100.0 * (deltaTotal - deltaIdle) / deltaTotal;
    return fmt::format("CPU: {:.2f}%", cpuUsage);
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

std::string ResourceMonitor::getDiskIo() {
    std::ifstream proc_diskstats("/proc/diskstats");
    if (!proc_diskstats) return "Disk: ?";

    std::string line;
    std::map<std::string, uint64_t> diskIoTime;

    auto now = std::chrono::system_clock::now();
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

    OnScopeExit onScopeExit([&]() {
        impl_->diskIoUpdateTime_ = now;
        impl_->diskIoTime_ = std::move(diskIoTime);
    });

    if(!impl_->diskIoUpdateTime_.has_value()) {
        return "Disk: ?";
    }

    // 计算磁盘繁忙百分比
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

    return result;
}

std::vector<std::string> ResourceMonitor::getTopCpuProcesses(int numProcesses, double minCpuUsage) {
    std::vector<std::string> topCPUs;
    auto readCPUTotal = []() -> uint64_t {
        std::ifstream file("/proc/stat");
        if (!file.is_open()) return 0;
    
        std::string line;
        std::getline(file, line); // first line should start with "cpu"
        std::istringstream iss(line);
    
        std::string label;
        iss >> label; // skip "cpu"
    
        uint64_t val, sum = 0;
        while (iss >> val) {
            sum += val;
        }
    
        return sum;
    };

    std::map<int, Impl::ProcessTime> processTimes;

    // 遍历/proc目录获取所有进程
    for (const auto& entry : std::filesystem::directory_iterator("/proc")) {
        try {
            // 检查是否是进程目录(数字命名的目录)
            std::string pidStr = entry.path().filename();
            if (std::all_of(pidStr.begin(), pidStr.end(), ::isdigit)) {
                int pid = std::stoi(pidStr);
                std::ifstream statFile(entry.path() / "stat");
                std::string statLine;
                if (std::getline(statFile, statLine)) {
                    std::istringstream iss(statLine);
                    std::vector<std::string> tokens{
                        std::istream_iterator<std::string>{iss},
                        std::istream_iterator<std::string>{}
                    };

                    if (tokens.size() >= 22) {
                        // 计算CPU使用率
                        uint64_t utime = std::stoull(tokens[13]);
                        uint64_t stime = std::stoull(tokens[14]);
                        //uint64_t cutime = std::stoull(tokens[15]);
                        //uint64_t cstime = std::stoull(tokens[16]);
                        //uint64_t starttime = std::stoull(tokens[21]);
                        //uint64_t total_time = utime + stime + cutime + cstime;

                        uint64_t totalTime = utime + stime;
                        
                        processTimes[pid] = {
                            pid,
                            totalTime
                        };
                    }
                }
            }
        } catch (...) {
            continue; // 跳过无法访问的进程目录
        }
    }

    auto cpuTime = readCPUTotal();

    OnScopeExit onScopeExit([&]() {
        impl_->prevCpuTime_ = cpuTime;
        impl_->processTimes_ = std::move(processTimes);
    });

    if(!impl_->prevCpuTime_.has_value()) {
        return topCPUs;
    }

    // 计算CPU使用率
    std::multimap<uint64_t, int> topCPUMap;  // key: deltaTotalTime, value: pid
    for(auto &process: processTimes) {
        auto prev = impl_->processTimes_.find(process.first);
        if(prev != impl_->processTimes_.end()) {
            auto prevTotalTime = prev->second.totalTime_;
            auto deltaTotalTime = process.second.totalTime_ - prevTotalTime;
            topCPUMap.emplace(deltaTotalTime, process.first);
        }
    }

    // 排序并获取前numProcesses个进程
    auto deltaCpuTime = cpuTime - impl_->prevCpuTime_.value();
    int n = 0;
    for(auto it = topCPUMap.rbegin(); it != topCPUMap.rend() && n < numProcesses; ++it, ++n) {
        double cpuUsage = it->first / (double)deltaCpuTime;
        //std::cout << "it->first: " << it->first << " deltaCpuTime: " << deltaCpuTime << std::endl;
        if(cpuUsage < minCpuUsage) continue;

        auto pid = it->second;
        auto cmdline = getCmdLine(pid);
        topCPUs.emplace_back(fmt::format("CPU: {:.2f}%, CMD: [{}]{}", cpuUsage*100, pid, cmdline));
    }

    return topCPUs;
}


std::vector<std::string> ResourceMonitor::getTopMemProcesses(int numProcesses, uint64_t minMemUsage) {
    std::vector<std::string> topMemories;

    // 添加内存统计
    std::multimap<uint64_t, int> memoryMap;  // key: 内存大小(字节), value: pid
    for (const auto& entry : std::filesystem::directory_iterator("/proc")) {
        try {
            std::string pidStr = entry.path().filename();
            if (std::all_of(pidStr.begin(), pidStr.end(), ::isdigit)) {
                int pid = std::stoi(pidStr);
                
                // 读取/proc/[pid]/statm获取内存信息
                std::ifstream statmFile(entry.path() / "statm");
                if (statmFile) {
                    uint64_t size, resident, shared, text, lib, data, dt;
                    statmFile >> size >> resident >> shared >> text >> lib >> data >> dt;
                    
                    // resident是实际驻留内存大小(页数)
                    uint64_t rss = resident * sysconf(_SC_PAGESIZE); // 转换为字节
                    memoryMap.emplace(rss, pid);
                }
            }
        } catch (...) {
            continue;
        }
    }

    // 获取内存占用最高的进程  
    int n = 0;
    for (auto it = memoryMap.rbegin(); it != memoryMap.rend() && n < numProcesses; ++it, ++n) {
        auto pid = it->second;
        auto cmdline = getCmdLine(pid);
        auto memorySize = it->first;
        if(memorySize < minMemUsage) continue;

        topMemories.emplace_back(fmt::format("MEM: {}, CMD: [{}]{}", 
            pid, valueToHumanReadable(memorySize), pid, cmdline));
    }

    return topMemories;
}

std::vector<std::string> ResourceMonitor::getTopDiskProcesses(int numProcesses, uint64_t minDiskUsage) {
    //std::vector<std::string> &topMemories,
    std::vector<std::string> topDiskIos;

    // 添加磁盘IO统计  
    std::map<int, Impl::ProcessIo> processIos;
    for (const auto& entry : std::filesystem::directory_iterator("/proc")) {
        try {
            std::string pidStr = entry.path().filename();
            if (std::all_of(pidStr.begin(), pidStr.end(), ::isdigit)) {
                int pid = std::stoi(pidStr);
                
                // 读取/proc/[pid]/io获取IO信息
                std::ifstream ioFile(entry.path() / "io");
                if (ioFile) {
                    uint64_t readBytes = 0;
                    uint64_t writeBytes = 0;
                    std::string line;
                    
                    while (std::getline(ioFile, line)) {
                        if (line.find("read_bytes:") == 0) {
                            readBytes = std::stoull(line.substr(11));
                        } else if (line.find("write_bytes:") == 0) {
                            writeBytes = std::stoull(line.substr(12));
                        }
                    }

                    processIos[pid] = {
                        pid,
                        readBytes,
                        writeBytes
                    };
                }
            }
        } catch (...) {
            continue;
        }
    }

    OnScopeExit onScopeExit([&]() {
        impl_->processIos_ = std::move(processIos);
    });    

    // 计算间隔时间内的磁盘IO总量 
    struct IoData {
        int pid_;
        uint64_t readBytes_;
        uint64_t writeBytes_;
    };
    std::multimap<uint64_t, IoData> ioMap;  // key: IO读写总量(字节), value: pid

    //std::cout << "processIos = " << processIos.size() << "\n";
    for(auto &process: processIos) {
        
        auto prev = impl_->processIos_.find(process.first);
        if(prev != impl_->processIos_.end()) {
            //std::cout << "pid = " << process.first << "\n";
            auto prevReadBytes = prev->second.readIo_;
            auto prevWriteBytes = prev->second.writeIo_;
            auto readBytes = process.second.readIo_;
            auto writeBytes = process.second.writeIo_;
            auto deltaReadBytes = readBytes - prevReadBytes;
            auto deltaWriteBytes = writeBytes - prevWriteBytes;
            uint64_t totalIO = deltaReadBytes + deltaWriteBytes;
            //if (totalIO > 0) 
            {
                ioMap.emplace(totalIO, IoData{
                    process.first,
                    deltaReadBytes,
                    deltaWriteBytes
                });
            }
        }
    }
    //std::cout << "ioMap = " << ioMap.size() << "\n";

    // 获取磁盘IO最高的进程
    int n = 0;
    for (auto it = ioMap.rbegin(); it != ioMap.rend() && n < numProcesses; ++it, ++n) {
        auto pid = it->second.pid_;
        auto deltaReadBytes = it->second.readBytes_;
        auto deltaWriteBytes = it->second.writeBytes_;
        auto totalIO = it->first;
        if(totalIO < minDiskUsage) continue;

        auto cmdline = getCmdLine(pid);
        
        topDiskIos.emplace_back(fmt::format("IO: {}+{}, CMD: [{}]{}",
            valueToHumanReadable(deltaReadBytes),
            valueToHumanReadable(deltaWriteBytes),
            pid,
            cmdline));
    }

    return topDiskIos;
}
