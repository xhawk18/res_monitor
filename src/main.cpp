#include "resource_monitor.h"
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

int main() {
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

        std::vector<std::string> topCPUs = monitor.getTopCpuProcesses(3);
        std::vector<std::string> topMemories = monitor.getTopMemProcesses(3);
        std::vector<std::string> topDiskIos = monitor.getTopDiskProcesses(3);
        
        for(const auto& process : topCPUs) {
            logger.info("Top CPU: {}", process);
        }

        for(const auto& process : topMemories) {
            logger.info("Top MEM: {}", process);
        }

        for(const auto& process : topDiskIos) {
            logger.info("Top Disk: {}", process);
        }

        // 可中断的睡眠
        std::unique_lock<std::mutex> lock(g_mutex);
        g_cv.wait_for(lock, std::chrono::seconds(10), []{ return g_stopping.load(); });
        //std::this_thread::sleep_for(std::chrono::seconds(10));
    }

    logger.info("Stopping...");
    logger.flush();
    spdlog::shutdown();
    return 0;
}