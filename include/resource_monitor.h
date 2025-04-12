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
    
    void getTopProcesses(
        int numProcesses,
        std::vector<std::string> &topCPUs,
        std::vector<std::string> &topMemories,
        std::vector<std::string> &topDiskIos    
    );
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
