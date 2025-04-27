#include "scheduler.h"
#include "scheduler_types.h"
#include <iostream>
#include <vector>
#include <queue>
#include <string>
#include <sstream>
#include <map>
#include <ctime>
#include <chrono>
#include <algorithm>
#include <iomanip>
#include <functional>
#include <fstream>
#include <memory>
#include <cstring>
#include <unistd.h>
#include <sys/stat.h>

// Utility functions
std::string current_time_str() {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_time_t), "%H:%M:%S");
    return ss.str();
}

// String conversions for enums
std::string to_string(SchedulerType type) {
    return type == LINUX ? "Linux" : "Android";
}

std::string to_string(LinuxClass cls) {
    switch (cls) {
        case LINUX_FOREGROUND: return "Foreground";
        case LINUX_BACKGROUND: return "Background";
        case LINUX_DAEMON: return "Daemon";
        case LINUX_EMPTY: return "Empty";
        default: return "Unknown";
    }
}

std::string to_string(SchedulingPolicy policy) {
    switch (policy) {
        case POLICY_FIFO: return "FIFO";
        case POLICY_ROUND_ROBIN: return "Round Robin";
        case POLICY_TIME_SHARING: return "Time Sharing";
        case POLICY_IDLE: return "Idle";
        case POLICY_DEADLINE: return "Deadline";
        default: return "Unknown";
    }
}

std::string to_string(AndroidClass cls) {
    switch (cls) {
        case ANDROID_FOREGROUND: return "Foreground";
        case ANDROID_VISIBLE: return "Visible";
        case ANDROID_SERVICE: return "Service";
        case ANDROID_BACKGROUND: return "Background";
        case ANDROID_CACHED: return "Cached";
        default: return "Unknown";
    }
}

// Include the rest of scheduler.cpp implementation here
// ...

// The full implementation of scheduler.cpp should go here

// Parsing functions
LinuxClass parse_linux_class(const std::string& str) {
    if (str == "fg") return LINUX_FOREGROUND;
    if (str == "bg") return LINUX_BACKGROUND;
    if (str == "daemon") return LINUX_DAEMON;
    if (str == "empty") return LINUX_EMPTY;
    // Default to foreground
    return LINUX_FOREGROUND;
}

SchedulingPolicy parse_scheduling_policy(const std::string& str) {
    if (str == "fifo") return POLICY_FIFO;
    if (str == "rr") return POLICY_ROUND_ROBIN;
    if (str == "ts") return POLICY_TIME_SHARING;
    if (str == "idle") return POLICY_IDLE;
    if (str == "deadline") return POLICY_DEADLINE;
    // Default to time sharing
    return POLICY_TIME_SHARING;
}

AndroidClass parse_android_class(const std::string& str) {
    if (str == "fg") return ANDROID_FOREGROUND;
    if (str == "vis") return ANDROID_VISIBLE;
    if (str == "svc") return ANDROID_SERVICE;
    if (str == "bg") return ANDROID_BACKGROUND;
    if (str == "cache") return ANDROID_CACHED;
    // Default to foreground
    return ANDROID_FOREGROUND;
}

void show_help() {
    std::cout << "Available commands:" << std::endl;
    std::cout << "  create <name> <burst_time> <nice_value> [scheduler_type] [class] [policy]" << std::endl;
    std::cout << "    Creates a new task with specified parameters" << std::endl;
    std::cout << "    scheduler_type: linux (default) or android" << std::endl;
    std::cout << "    class: For Linux - fg (foreground), bg (background), daemon, empty" << std::endl;
    std::cout << "           For Android - fg (foreground), vis (visible), svc (service)," << std::endl;
    std::cout << "                         bg (background), cache (cached)" << std::endl;
    std::cout << "    policy: fifo, rr (round robin), ts (time sharing), idle, deadline" << std::endl;
    std::cout << std::endl;
    std::cout << "  run_linux" << std::endl;
    std::cout << "    Runs the Linux simulation until all tasks complete" << std::endl;
    std::cout << std::endl;
    std::cout << "  run_android" << std::endl;
    std::cout << "    Runs the Android simulation until all tasks complete" << std::endl;
    std::cout << std::endl;
    std::cout << "  step [n]" << std::endl;
    std::cout << "    Advances the simulation by n milliseconds (default: 10)" << std::endl;
    std::cout << std::endl;
    std::cout << "  ts" << std::endl;
    std::cout << "    Lists all tasks (similar to ps command)" << std::endl;
    std::cout << std::endl;
    std::cout << "  use <linux|android>" << std::endl;
    std::cout << "    Switches to the specified scheduler type" << std::endl;
    std::cout << std::endl;
    std::cout << "  status" << std::endl;
    std::cout << "    Shows current state of all queues and running tasks" << std::endl;
    std::cout << std::endl;
    std::cout << "  stats" << std::endl;
    std::cout << "    Shows performance statistics" << std::endl;
    std::cout << std::endl;
    std::cout << "  help" << std::endl;
    std::cout << "    Displays this help message" << std::endl;
    std::cout << std::endl;
    std::cout << "  exit, quit" << std::endl;
    std::cout << "    Exits the simulator" << std::endl;
} 