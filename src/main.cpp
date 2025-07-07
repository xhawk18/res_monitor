#include "resource_monitor.h"
#include <docopt.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <chrono>
#include <thread>
#include <filesystem>
#include <csignal>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <iostream>

namespace fs = std::filesystem;

struct Global {
    std::atomic<bool> stopping_{false};
    std::mutex mutex_;
    std::condition_variable cv_;
};

Global &getGlobal() {
    static Global s_global;
    return s_global;
}

void signalHandler(int signum) {
    auto &global = getGlobal();
    global.stopping_ = true;
    global.cv_.notify_all();  // 唤醒所有等待的线程
}

static const char USAGE[] =
R"(资源监控工具

Usage:
  res_monitor [-i <interval>] [-c <min_cpu>] [-m <min_mem>] [-d <min_disk>] [-n <num_processes>]
  res_monitor (-h | --help)

Options:
  -i <interval>       更新间隔(秒) [默认: 10]
  -c <min_cpu>        最小CPU使用率(%) [默认: 1]
  -m <min_mem>        最小内存使用量(MB) [默认: 1]
  -d <min_disk>       最小磁盘IO(KB/s) [默认: 1]
  -n <num_processes>  显示进程数 [默认: 3]
  -h --help           显示帮助信息
)";

//#define PTI do {SPDLOG_INFO("");} while(0)

int main(int argc, char** argv) {
    // 注册信号处理
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    // 创建控制台和轮转文件日志器
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

    // 获取可执行文件当前目录
    fs::path currentPath = std::filesystem::current_path();
    fs::path logPath = currentPath / "logs";
    // 创建轮转文件日志器
    std::filesystem::create_directories(logPath);
    fs::path logFilePath = logPath / "monitor.log";

    auto rotating_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
        logFilePath, 
        50 * 1024 * 1024, // 每个文件最大50MB
        10  // 保留10个文件
    );
    
    auto logger = std::make_shared<spdlog::logger>(spdlog::logger{"multi_sink", {console_sink, rotating_sink}});
    spdlog::set_default_logger(logger);
    logger->set_level(spdlog::level::info);
    //logger.set_pattern("[%Y-%m-%d %T.%f] [%L] [%t] [%s:%#:%!] %^%v%#$");

    // 解析命令行参数 
    auto args = docopt::docopt(USAGE, {argv + 1, argv + argc}, true);

    uint64_t interval = 10; // 10s
    uint64_t minCpu = 1;    // 1%
    uint64_t minMem = 1;    // 1MB
    uint64_t minDisk = 1;   // 1KB
    uint64_t numProcesses = 3;  // 3
    
    auto getArg = [&args](const std::string& key, uint64_t *result) {
        const auto &value = args[key];
        if (value.isLong()) {
            *result = value.asLong();
        }
        else if(value.isString()) {
            *result = std::stoull(value.asString());
        }
    };

    try {
        getArg("-i", &interval);
        getArg("-c", &minCpu);
        getArg("-m", &minMem);
        getArg("-d", &minDisk);
        getArg("-n", &numProcesses);
    } catch (const std::exception& e) {
        SPDLOG_ERROR("参数解析错误: {}", e.what());
        return 1;
    }

    SPDLOG_INFO("interval: {}sec, minCpu: {}%, minMem: {}M, minDisk: {}k, numProcesses: {}",
        interval,
        minCpu,
        minMem,
        minDisk,
        numProcesses);

    ResourceMonitor monitor;
    auto &global = getGlobal();
    while(!global.stopping_) {
        auto cpuUsage = monitor.getCpuUsage();
        auto memUsage = monitor.getMemoryUsage();
        auto diskIo = monitor.getDiskIo();
        auto temperature = monitor.getTemperature();
        
        SPDLOG_INFO("{}, {}, {}", cpuUsage, memUsage, diskIo);
        SPDLOG_INFO("\n{}", temperature);

        auto topCPUs = monitor.getTopCpuProcesses(numProcesses, minCpu/100.0);
        auto topMemories = monitor.getTopMemProcesses(numProcesses, minMem*1024*1024);
        auto topDiskIos = monitor.getTopDiskProcesses(numProcesses, minDisk*1024);
        
        for(const auto& process : topCPUs) {
            SPDLOG_INFO("{}", process);
        }

        for(const auto& process : topMemories) {
            SPDLOG_INFO("{}", process);
        }

        for(const auto& process : topDiskIos) {
            SPDLOG_INFO("{}", process);
        }

        // 可中断的睡眠
        std::unique_lock<std::mutex> lock(global.mutex_);
        global.cv_.wait_for(lock, std::chrono::seconds(interval), [&global]{
            return global.stopping_.load();
        });
    }

    SPDLOG_INFO("Stopping...");
    logger->flush();
    spdlog::shutdown();
    return 0;
}
