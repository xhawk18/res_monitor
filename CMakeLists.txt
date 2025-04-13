cmake_minimum_required(VERSION 3.10)
project(res_monitor)

set(CMAKE_CXX_STANDARD 17)

# 设置静态编译选项
set(CMAKE_EXE_LINKER_FLAGS "-static")
set(BUILD_SHARED_LIBS OFF)

# 使用本地spdlog代替系统查找
add_subdirectory(libs/spdlog)

# 添加可执行文件
add_executable(res_monitor src/main.cpp src/resource_monitor.cpp)

# 添加头文件路径
target_include_directories(res_monitor PRIVATE include)

# 修改链接目标为本地spdlog并静态链接
target_link_libraries(res_monitor PRIVATE spdlog::spdlog_header_only)