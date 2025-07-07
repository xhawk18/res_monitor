#pragma once
#include <string>
#include <memory>
#include <vector>

class ResourceMonitor {
public:
    ResourceMonitor();
    ~ResourceMonitor();

    std::string getCpuUsage();
    std::string getMemoryUsage();
    std::string getDiskIo();
    std::string getTemperature();
    
    std::vector<std::string> getTopCpuProcesses(int numProcesses, double minCpuUsage = 0.01);
    std::vector<std::string> getTopMemProcesses(int numProcesses, uint64_t minMemUsage = 1024*1024);
    std::vector<std::string> getTopDiskProcesses(int numProcesses, uint64_t minDiskUsage = 1024);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
