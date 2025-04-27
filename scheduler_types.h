#ifndef SCHEDULER_TYPES_H
#define SCHEDULER_TYPES_H

// Enums for scheduler type, classes, and policies
enum SchedulerType {
    LINUX,
    ANDROID
};

enum LinuxClass {
    LINUX_FOREGROUND,
    LINUX_BACKGROUND,
    LINUX_DAEMON,
    LINUX_EMPTY
};

// Common scheduling policies for both Linux and Android
enum SchedulingPolicy {
    POLICY_FIFO,        // Highest priority, no preemption
    POLICY_ROUND_ROBIN, // High priority, time-sliced
    POLICY_TIME_SHARING,// Normal priority, dynamic priority adjustments
    POLICY_IDLE,        // Lowest priority
    POLICY_DEADLINE     // Real-time deadline-based
};

enum AndroidClass {
    ANDROID_FOREGROUND,
    ANDROID_VISIBLE,
    ANDROID_SERVICE,
    ANDROID_BACKGROUND,
    ANDROID_CACHED
};

#endif // SCHEDULER_TYPES_H 