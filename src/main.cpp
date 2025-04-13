#include "resource_monitor.h"
#include <docopt/docopt.h>
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <chrono>
#include <thread>
#include <filesystem>
#include <csignal>
#include <atomic>

namespace fs = std::filesystem;

std::atomic<bool> g_stopping{false};
std::mutex g_mutex;
std::condition_variable g_cv;

void signalHandler(int signum) {
    g_stopping = true;
    g_cv.notify_all();  // 唤醒所有等待的线程
}

static const char USAGE[] =
R"(资源监控工具

用法:
  res_monitor [-i <interval>] [-mc <min_cpu>] [-mm <min_mem>] [-md <min_disk>]
  res_monitor (-h | --help)

选项:
  -i <interval>  更新间隔(秒) [默认: 10]
  -mc <min_cpu>  最小CPU使用率(%) [默认: 1]
  -mm <min_mem>  最小内存使用量(MB) [默认: 1]
  -md <min_disk> 最小磁盘IO(KB) [默认: 1]
  -h --help      显示帮助信息
)";

int main(int argc, char** argv) {
    auto args = docopt::docopt(USAGE, {argv + 1, argv + argc}, true);

    int interval = 10;
    double minCpu = 1.0;
    uint64_t minMem = 1024 * 1024; // 1MB
    uint64_t minDisk = 1024; // 1KB

    try {
        interval = std::stoi(args["-i"].asString());
        minCpu = std::stod(args["-mc"].asString());
        minMem = std::stoull(args["-mm"].asString()) * 1024 * 1024;
        minDisk = std::stoull(args["-md"].asString()) * 1024;
    } catch (...) {
        spdlog::error("参数解析错误");
        return 1;
    }

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
    
    spdlog::logger logger("multi_sink", {console_sink, rotating_sink});
    logger.set_level(spdlog::level::info);

    ResourceMonitor monitor;
    while(!g_stopping) {
        auto cpuUsage = monitor.getCpuUsage();
        auto memUsage = monitor.getMemoryUsage();
        auto diskIo = monitor.getDiskIo();
        
        logger.info("{}, {}, {}", cpuUsage, memUsage, diskIo);

        auto topCPUs = monitor.getTopCpuProcesses(3, minCpu);
        auto topMemories = monitor.getTopMemProcesses(3, minMem);
        auto topDiskIos = monitor.getTopDiskProcesses(3, minDisk);
        
        for(const auto& process : topCPUs) {
            logger.info("{}", process);
        }

        for(const auto& process : topMemories) {
            logger.info("{}", process);
        }

        for(const auto& process : topDiskIos) {
            logger.info("{}", process);
        }

        // 可中断的睡眠
        std::unique_lock<std::mutex> lock(g_mutex);
        g_cv.wait_for(lock, std::chrono::seconds(interval), []{return g_stopping.load();});
    }

    logger.info("Stopping...");
    logger.flush();
    spdlog::shutdown();
    return 0;
}