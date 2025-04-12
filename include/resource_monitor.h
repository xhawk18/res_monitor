#pragma once
#include <string>
#include <memory>

class ResourceMonitor {
public:
    ResourceMonitor();
    ~ResourceMonitor();

    std::string getCpuUsage();
    std::string getMemoryUsage();
    std::string getDiskUsage();
    
private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
