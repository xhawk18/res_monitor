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

    while(true) {
        double cpu_usage = ResourceMonitor::get_cpu_usage();
        double mem_usage = ResourceMonitor::get_memory_usage();
        
        logger.info("CPU Usage: {:.2f}%", cpu_usage);
        logger.info("Memory Usage: {:.2f}%", mem_usage);
        
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }

    return 0;
}