cmake_minimum_required(VERSION 3.10)
project(res_monitor)

set(CMAKE_CXX_STANDARD 20)

# 设置静态编译选项
set(CMAKE_EXE_LINKER_FLAGS "-static")
set(BUILD_SHARED_LIBS OFF)

# 使用本地spdlog代替系统查找
add_subdirectory(libs/spdlog)

# 添加docopt库
add_subdirectory(libs/docopt.cpp)

# 修改可执行文件配置
add_executable(res_monitor src/main.cpp src/resource_monitor.cpp)
target_include_directories(res_monitor PRIVATE include)
target_link_libraries(res_monitor PRIVATE spdlog::spdlog_header_only docopt)