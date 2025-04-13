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
    
    std::vector<std::string> getTopCpuProcesses(int numProcesses);
    std::vector<std::string> getTopMemProcesses(int numProcesses);
    std::vector<std::string> getTopDiskProcesses(int numProcesses);
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
