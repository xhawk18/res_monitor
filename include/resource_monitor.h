#pragma once
#include <string>

class ResourceMonitor {
public:
    static double get_cpu_usage();
    static double get_memory_usage();
};
