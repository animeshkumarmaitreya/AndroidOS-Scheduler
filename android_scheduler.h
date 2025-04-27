#ifndef ANDROID_SCHEDULER_H
#define ANDROID_SCHEDULER_H

#include <sys/types.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// Process states (matching Android's process lifecycle)
typedef enum {
    PROCESS_STATE_FOREGROUND,    // Active, visible process
    PROCESS_STATE_VISIBLE,       // Visible but not focused
    PROCESS_STATE_SERVICE,       // Running service
    PROCESS_STATE_BACKGROUND,    // Background process
    PROCESS_STATE_CACHED         // In memory but inactive
} ProcessState;

// Function declarations
void log_message(const char *format, ...);
int assign_to_cgroup(const char *cgroup_path, pid_t pid);
float get_process_cpu_usage(pid_t pid);
long get_process_memory_usage(pid_t pid);
pid_t get_focused_window_pid();
bool is_playing_audio(pid_t pid);
bool is_using_gpu(pid_t pid);
bool check_memory_pressure();
void setup_cgroups();
void attach_to_existing_processes();
void monitor_all_processes();
void setup_priority_change_service();
void check_priority_requests();
void launch_and_track_process(const char *group, char *const argv[]);
void set_oom_score(pid_t pid, int score);

// Main function that can be called from C++
int android_scheduler_main(int argc, char *argv[]);

#ifdef __cplusplus
}
#endif

// C++ wrapper function
#ifdef __cplusplus
int run_android_process_scheduler(int argc, char *argv[]);
#endif

#endif // ANDROID_SCHEDULER_H 