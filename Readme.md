# Resource Monitor 
[chinese version (中文版)](Readme_cn.md)

## Overview
A Linux system resource monitoring tool that displays real-time CPU, memory and disk usage, and lists processes with highest resource consumption.

This tool helps identify system bottlenecks by analyzing logged data when encountering high system load or performance issues.

## Key Features
- ✅ Real-time CPU usage monitoring
- ✅ Real-time memory usage monitoring (including swap space)
- ✅ Real-time disk I/O activity monitoring
- ✅ Show top CPU consuming processes (with configurable minimum CPU usage threshold)
- ✅ Show top memory consuming processes (with configurable minimum memory usage threshold)
- ✅ Show top disk I/O processes (with configurable minimum I/O threshold)
- ✅ Logging functionality (console output + file rotation)

## Usage

```bash
Usage:
  res_monitor [-i <interval>] [-c <min_cpu>] [-m <min_mem>] [-d <min_disk>] [-n <num_processes>]
  res_monitor (-h | --help)

Options:
  -i <interval>       Update interval in seconds [default: 10]
  -c <min_cpu>        Minimum CPU usage percentage [default: 1]
  -m <min_mem>        Minimum memory usage in MB [default: 1]
  -d <min_disk>       Minimum disk I/O in KB/s [default: 1]
  -n <num_processes>  Number of processes to display [default: 3]
  -h --help           Show help message
