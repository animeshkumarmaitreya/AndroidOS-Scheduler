#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <string>
#include <memory>
#include "scheduler_types.h"

// Forward declarations
class Task;
class Scheduler;
class LinuxScheduler;
class AndroidScheduler;
class Simulator;

// String conversion functions
std::string to_string(SchedulerType type);
std::string to_string(LinuxClass cls);
std::string to_string(SchedulingPolicy policy);
std::string to_string(AndroidClass cls);

// Parsing functions
LinuxClass parse_linux_class(const std::string& str);
SchedulingPolicy parse_scheduling_policy(const std::string& str);
AndroidClass parse_android_class(const std::string& str);
void show_help();

// Simulator wrapper function
int run_linux_android_simulator(int argc, char** argv);

#endif // SCHEDULER_H 