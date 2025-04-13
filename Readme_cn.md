# 资源监控工具 (Resource Monitor)

## 功能概述
一个linux下的系统资源监控工具，实时显示CPU、内存和磁盘使用情况，并能列出资源占用最高的进程。

## 主要功能
- ✅ 实时监控CPU使用率
- ✅ 实时监控内存使用情况（包括交换分区）
- ✅ 实时监控磁盘I/O活动
- ✅ 显示CPU占用最高的几个进程（可设置最小CPU使用率阈值）
- ✅ 显示内存占用最高的几个进程（可设置最小内存使用阈值）
- ✅ 显示磁盘I/O最高的几个进程（可设置最小I/O阈值）
- ✅ 日志记录功能（控制台输出+文件轮转）

## 使用说明

```bash
Usage:
  res_monitor [-i <interval>] [-c <min_cpu>] [-m <min_mem>] [-d <min_disk>] [-n <num_processes>]
  res_monitor (-h | --help)

Options:
  -i <interval>       更新间隔(秒) [默认: 10]
  -c <min_cpu>        最小CPU使用率(%) [默认: 1]
  -m <min_mem>        最小内存使用量(MB) [默认: 1]
  -d <min_disk>       最小磁盘IO(KB/s) [默认: 1]
  -n <num_processes>  显示进程数 [默认: 3]
  -h --help           显示帮助信息
