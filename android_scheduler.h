#ifndef ANDROID_SCHEDULER_H
#define ANDROID_SCHEDULER_H

#include <sys/types.h>
#include <stdbool.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

// Process priority levels
#define PRIORITY_FOREGROUND 0    // Visible activities
#define PRIORITY_VISIBLE 1       // Visible but not in focus
#define PRIORITY_SERVICE 2       // Background services
#define PRIORITY_BACKGROUND 3    // Background processes
#define PRIORITY_EMPTY 4         // Empty process kept for caching

// Process states (matching Android's process lifecycle)
typedef enum {
    PROCESS_STATE_FOREGROUND,    // Active, visible process
    PROCESS_STATE_VISIBLE,       // Visible but not focused
    PROCESS_STATE_SERVICE,       // Running service
    PROCESS_STATE_BACKGROUND,    // Background process
    PROCESS_STATE_CACHED         // In memory but inactive
} ProcessState;

// Monitoring configuration
#define MONITOR_INTERVAL 2
#define CPU_HISTORY_SIZE 10
#define MEM_HISTORY_SIZE 10
#define MAX_PROCESSES 128
#define LOW_MEMORY_THRESHOLD 15  // 15% available memory threshold

// cgroup paths
#define CGROUP_FOREGROUND "/sys/fs/cgroup/foreground"
#define CGROUP_VISIBLE "/sys/fs/cgroup/visible"
#define CGROUP_SERVICE "/sys/fs/cgroup/service" 
#define CGROUP_BACKGROUND "/sys/fs/cgroup/background"
#define CGROUP_CACHED "/sys/fs/cgroup/cached"

// Process resource usage history
typedef struct {
    float cpu_usage[CPU_HISTORY_SIZE];
    int cpu_index;
    long memory_usage[MEM_HISTORY_SIZE];
    int mem_index;
    time_t last_network_activity;
    time_t last_disk_activity;
    time_t last_gpu_activity;
} ResourceHistory;

// Process tracking structure
typedef struct {
    pid_t pid;
    time_t last_active;
    ProcessState state;
    char name[256];
    char cmdline[512];
    ResourceHistory resource_history;
    float importance_score;
    char cgroup_path[256];
    bool is_system_service;
    bool is_playing_audio;
    int requested_priority;
    time_t last_foreground_time;
    int oom_score;
} TrackedProcess;

// Function declarations
void log_message(const char *format, ...);
int assign_to_cgroup(const char *cgroup_path, pid_t pid);
float get_process_cpu_usage(pid_t pid);
long get_process_memory_usage(pid_t pid);
pid_t get_focused_window_pid();
bool is_playing_audio(pid_t pid);
pid_t get_parent_pid(pid_t pid);
bool is_using_gpu(pid_t pid);
bool check_disk_activity(pid_t pid);
bool is_using_network(pid_t pid);
bool check_memory_pressure();
bool is_system_service(pid_t pid);
void set_oom_score(pid_t pid, int score);
void update_resource_history(TrackedProcess *proc);
float calculate_average_cpu(TrackedProcess *proc);
long calculate_average_memory(TrackedProcess *proc);
float calculate_importance_score(TrackedProcess *proc, pid_t focused_pid);
int change_process_priority(pid_t pid, int requested_priority);
const char* get_cgroup_for_state(ProcessState state);
int get_oom_score_for_state(ProcessState state);
void update_process_state(TrackedProcess *proc, float importance_score);
void adjust_resource_controls(TrackedProcess *proc);
void update_lru_list();
void monitor_all_processes();
void initialize_process(TrackedProcess *proc, pid_t pid, const char *initial_group);
void launch_and_track_process(const char *group, char *const argv[]);
void setup_cgroups();
void attach_to_existing_processes();
bool are_processes_related(pid_t pid1, pid_t pid2);
bool check_ipc_connections(pid_t pid1, pid_t pid2);
void handle_signal(int sig);
void setup_priority_change_service();
void check_priority_requests();

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