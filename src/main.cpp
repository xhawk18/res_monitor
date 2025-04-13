#include "resource_monitor.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <chrono>
#include <thread>

int main() {
    // 创建控制台和文件日志器
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>("/home/user/Work/res_monitor/logs/monitor.log");
    
    spdlog::logger logger("multi_sink", {console_sink, file_sink});
    logger.set_level(spdlog::level::info);

    ResourceMonitor monitor;
    while(true) {
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
        std::this_thread::sleep_for(std::chrono::seconds(3));
    }

    return 0;
}