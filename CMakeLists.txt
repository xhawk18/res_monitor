cmake_minimum_required(VERSION 3.10)
project(res_monitor)

set(CMAKE_CXX_STANDARD 17)

# 查找spdlog包
find_package(spdlog REQUIRED)

# 添加可执行文件
add_executable(res_monitor src/main.cpp src/resource_monitor.cpp)

# 链接spdlog
target_link_libraries(res_monitor PRIVATE spdlog::spdlog)