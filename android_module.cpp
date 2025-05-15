#include "android_scheduler.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <dirent.h>
#include <stdbool.h>
#include <sys/sysinfo.h>
#include <math.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <signal.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>

/*
 * Android Process Scheduler
 * A simplified implementation of Android-style process management
 * 
 * Usage:
 *   ./android_scheduler [foreground|background] program [args...]
 *   ./android_scheduler (with no arguments to monitor existing processes)
 * 
 * Examples:
 *   ./android_scheduler foreground firefox    // Launches Firefox as foreground app
 *   ./android_scheduler background sleep 100  // Launches sleep as background app
 *   ./android_scheduler                       // Monitors all existing processes
 *
 * Process Priorities:
 *   - foreground: Visible apps the user is interacting with
 *   - visible: Visible but not in focus
 *   - service: Background services performing tasks
 *   - background: Background apps not visible
 *   - cached: Apps kept in memory but otherwise inactive
 * 
 * Control:
 *   - SIGUSR1: Prints debug information about tracked processes
 *   - SIGTERM/SIGINT: Cleans up and exits
 */

// Global variables
TrackedProcess processes[MAX_PROCESSES];
int process_count = 0;
int priority_request_fd = -1;
bool memory_pressure = false;
static bool should_exit = false;  // Flag to control the main loop

// Utility functions
void log_message(const char *format, ...) {
    va_list args;
    va_start(args, format);
    time_t now = time(NULL);
    char timestr[64];
    strftime(timestr, sizeof(timestr), "%Y-%m-%d %H:%M:%S", localtime(&now));
    printf("[%s] ", timestr);
    vprintf(format, args);
    printf("\n");
    va_end(args);
}

// Function implementations
int assign_to_cgroup(const char *cgroup_path, pid_t pid) {
    char procs_path[256];
    snprintf(procs_path, sizeof(procs_path), "%s/cgroup.procs", cgroup_path);

    int fd = open(procs_path, O_WRONLY);
    if (fd == -1) {
        log_message("Failed to open %s: %s", procs_path, strerror(errno));
        return -1;
    }

    char pid_str[32];
    snprintf(pid_str, sizeof(pid_str), "%d", pid);
    if (write(fd, pid_str, strlen(pid_str)) < 0) {
        log_message("Failed to write '%s' to %s: %s", pid_str, procs_path, strerror(errno));
        close(fd);
        return -1;
    }

    close(fd);
    return 0;
}

float get_process_cpu_usage(pid_t pid) {
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "ps -p %d -o %%cpu=", pid);
    FILE *fp = popen(cmd, "r");
    if (!fp) return 0.0;

    char buf[32];
    float cpu = 0.0;
    if (fgets(buf, sizeof(buf), fp)) {
        cpu = atof(buf);
    }
    pclose(fp);
    return cpu;
}

long get_process_memory_usage(pid_t pid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    FILE *fp = fopen(path, "r");
    if (!fp) return 0;

    char line[256];
    long rss = 0;
    while (fgets(line, sizeof(line), fp)) {
        if (strncmp(line, "VmRSS:", 6) == 0) {
            sscanf(line, "VmRSS: %ld", &rss);
            break;
        }
    }
    fclose(fp);
    return rss;
}

void update_resource_history(TrackedProcess *proc) {
    // Update CPU history
    float cpu = get_process_cpu_usage(proc->pid);
    proc->resource_history.cpu_usage[proc->resource_history.cpu_index] = cpu;
    proc->resource_history.cpu_index = (proc->resource_history.cpu_index + 1) % CPU_HISTORY_SIZE;
    
    // Update memory history
    long mem = get_process_memory_usage(proc->pid);
    proc->resource_history.memory_usage[proc->resource_history.mem_index] = mem;
    proc->resource_history.mem_index = (proc->resource_history.mem_index + 1) % MEM_HISTORY_SIZE;
    
    // Check for network activity
    if (is_using_network(proc->pid)) {
        proc->resource_history.last_network_activity = time(NULL);
    }
    
    // Update audio status
    proc->is_playing_audio = is_playing_audio(proc->pid);
    if (proc->is_playing_audio) {
        proc->last_active = time(NULL);
    }
    
    // Update GPU activity
    if (is_using_gpu(proc->pid)) {
        proc->resource_history.last_gpu_activity = time(NULL);
    }
}

float calculate_average_cpu(TrackedProcess *proc) {
    float total = 0.0;
    for (int i = 0; i < CPU_HISTORY_SIZE; i++) {
        total += proc->resource_history.cpu_usage[i];
    }
    return total / CPU_HISTORY_SIZE;
}

long calculate_average_memory(TrackedProcess *proc) {
    long total = 0;
    for (int i = 0; i < MEM_HISTORY_SIZE; i++) {
        total += proc->resource_history.memory_usage[i];
    }
    return total / MEM_HISTORY_SIZE;
}

pid_t get_focused_window_pid(void) {
    Display *display = XOpenDisplay(NULL);
    if (!display) {
        log_message("ERROR: Could not open X display");
        return -1;
    }

    Window root = DefaultRootWindow(display);
    Window focused = None;
    int revert_to;
    
    XGetInputFocus(display, &focused, &revert_to);
    if (focused == None) {
        log_message("ERROR: Could not get focused window");
        XCloseDisplay(display);
        return -1;
    }

    // Try to find the application window by traversing up
    Window parent = None;
    Window root_return;
    Window *children = NULL;
    unsigned int nchildren;
    
    // First attempt with _NET_WM_PID on the focused window
    Atom atom = XInternAtom(display, "_NET_WM_PID", True);
    if (atom != None) {
        Atom actual_type;
        int actual_format;
        unsigned long nitems, bytes_after;
        unsigned char *prop = NULL;

        if (XGetWindowProperty(display, focused, atom, 0, 1, False, XA_CARDINAL,
                              &actual_type, &actual_format, &nitems, &bytes_after, &prop) == Success && prop) {
            pid_t pid = *(pid_t *)prop;
            XFree(prop);
            XCloseDisplay(display);
            return pid;
        }
    }

    // If we couldn't get it directly, try traversing up to find an application window
    Window current = focused;
    while (current != None && current != root) {
        Window parent_return;
        Window *children_return;
        unsigned int nchildren_return;
        
        Status status = XQueryTree(display, current, &root_return, &parent_return, 
                                  &children_return, &nchildren_return);
        
        if (status == 0) {
            log_message("ERROR: XQueryTree failed");
            break;
        }
        
        if (children_return)
            XFree(children_return);
        
        // Try to get PID from this window
        Atom actual_type;
        int actual_format;
        unsigned long nitems, bytes_after;
        unsigned char *prop = NULL;
        
        if (XGetWindowProperty(display, current, atom, 0, 1, False, XA_CARDINAL,
                              &actual_type, &actual_format, &nitems, &bytes_after, &prop) == Success && prop) {
            pid_t pid = *(pid_t *)prop;
            XFree(prop);
            XCloseDisplay(display);
            return pid;
        }
        
        // Move up to parent
        current = parent_return;
    }

    // If all else fails, try to use xdotool
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "xdotool getwindowfocus getwindowpid 2>/dev/null");
    FILE *fp = popen(cmd, "r");
    if (fp) {
        char buf[32];
        if (fgets(buf, sizeof(buf), fp)) {
            pid_t pid = atoi(buf);
            if (pid > 0) {
                pclose(fp);
                XCloseDisplay(display);
                return pid;
            }
        }
        pclose(fp);
    }

    XCloseDisplay(display);
    log_message("ERROR: Could not determine PID of focused window");
    return -1;
}

pid_t get_parent_pid(pid_t pid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/status", pid);
    FILE *f = fopen(path, "r");
    if (!f) return -1;

    char line[256];
    pid_t ppid = -1;
    while (fgets(line, sizeof(line), f)) {
        if (sscanf(line, "PPid:\t%d", &ppid) == 1) break;
    }
    fclose(f);
    return ppid;
}

bool is_playing_audio(pid_t pid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/fd", pid);
    DIR *dir = opendir(path);
    if (!dir) return false;

    struct dirent *entry;
    char link_path[512], target[512];
    while ((entry = readdir(dir)) != NULL) {
        snprintf(link_path, sizeof(link_path), "%s/%s", path, entry->d_name);
        ssize_t len = readlink(link_path, target, sizeof(target) - 1);
        if (len != -1) {
            target[len] = '\0';
            if (strstr(target, "/snd/") || strstr(target, "/pulse/") || strstr(target, "/alsa/")) {
                closedir(dir);
                return true;
            }
        }
    }
    closedir(dir);
    return false;
}

bool is_using_gpu(pid_t pid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/fd", pid);
    DIR *dir = opendir(path);
    if (!dir) return false;

    struct dirent *entry;
    char link_path[512], target[512];
    while ((entry = readdir(dir)) != NULL) {
        snprintf(link_path, sizeof(link_path), "%s/%s", path, entry->d_name);
        ssize_t len = readlink(link_path, target, sizeof(target) - 1);
        if (len != -1) {
            target[len] = '\0';
            if (strstr(target, "/dri/") || strstr(target, "/nvidia") || 
                strstr(target, "/dev/dri/") || strstr(target, "/dev/nvidia")) {
                closedir(dir);
                return true;
            }
        }
    }
    closedir(dir);
    return false;
}

bool check_disk_activity(pid_t pid) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/io", pid);
    FILE *f = fopen(path, "r");
    if (!f) return false;

    char line[256];
    unsigned long read_bytes_old = 0, write_bytes_old = 0;
    unsigned long read_bytes_new = 0, write_bytes_new = 0;
    
    // Read current values
    while (fgets(line, sizeof(line), f)) {
        if (strncmp(line, "read_bytes:", 11) == 0)
            sscanf(line, "read_bytes: %lu", &read_bytes_new);
        else if (strncmp(line, "write_bytes:", 12) == 0)
            sscanf(line, "write_bytes: %lu", &write_bytes_new);
    }
    fclose(f);
    
    // Compare with previous values (stored elsewhere)
    return (read_bytes_new > read_bytes_old + 1024) || (write_bytes_new > write_bytes_old + 1024);
}

bool is_using_network(pid_t pid) {
    // Check TCP connections
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/net/tcp", pid);
    FILE *f = fopen(path, "r");
    if (f) {
        char line[512];
        int count = 0;
        while (fgets(line, sizeof(line), f)) count++;
        fclose(f);
        if (count > 1) return true;  // header + at least 1 connection
    }

    // Check UDP connections
    snprintf(path, sizeof(path), "/proc/%d/net/udp", pid);
    f = fopen(path, "r");
    if (f) {
        char line[512];
        int count = 0;
        while (fgets(line, sizeof(line), f)) count++;
        fclose(f);
        if (count > 1) return true;
    }
    
    return false;
}

bool check_memory_pressure(void) {
    struct sysinfo info;
    if (sysinfo(&info) != 0) return false;
    
    // Calculate free memory percentage
    long total_ram = info.totalram * info.mem_unit;
    long free_ram = info.freeram * info.mem_unit;
    long free_percentage = (free_ram * 100) / total_ram;
    
    return (free_percentage < LOW_MEMORY_THRESHOLD);
}

bool is_system_service(pid_t pid) {
    char cmdline[256];
    snprintf(cmdline, sizeof(cmdline), "/proc/%d/cmdline", pid);
    
    FILE *f = fopen(cmdline, "r");
    if (!f) return false;
    
    char buf[256];
    size_t len = fread(buf, 1, sizeof(buf) - 1, f);
    fclose(f);
    
    if (len > 0) {
        buf[len] = '\0';
        return (strstr(buf, "systemd") || strstr(buf, "dbus") || 
                strstr(buf, "networkmanager") || strstr(buf, "pulseaudio") ||
                strstr(buf, "pipewire") || strstr(buf, "Xorg") ||
                strstr(buf, "cupsd") || strstr(buf, "bluetoothd"));
    }
    return false;
}

void set_oom_score(pid_t pid, int score) {
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/oom_score_adj", pid);
    
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "%d", score);
        fclose(f);
    }
}

float calculate_importance_score(TrackedProcess *proc, pid_t focused_pid) {
    float score = 0.0;
    time_t now = time(NULL);
    
    // Process state base scores
    if (proc->pid == focused_pid) {
        score += 100.0;  // Focused process gets top priority
        proc->last_foreground_time = now;
    }
    
    // Check if child/parent of focused process
    pid_t parent = get_parent_pid(proc->pid);
    if (parent > 0 && parent == focused_pid) {
        score += 90.0;  // Child of focused process
    }
    
    // System service bonus
    if (proc->is_system_service) {
        score += 50.0;
    }
    
    // Activity bonuses
    if (proc->is_playing_audio) {
        score += 80.0;  // Audio playback is important
    }
    
    // Recent GPU activity is important for UI responsiveness
    if (now - proc->resource_history.last_gpu_activity < 5) {
        score += 40.0;
    }
    
    // Network activity suggests user interaction
    if (now - proc->resource_history.last_network_activity < 10) {
        score += 20.0;
    }
    
    // Recent activity score
    int idle_time = now - proc->last_active;
    if (idle_time < 30) {
        score += 30.0 * (1.0 - (idle_time / 30.0));
    }
    
    // Recent foreground score (process was recently in foreground)
    int fg_idle_time = now - proc->last_foreground_time;
    if (fg_idle_time < 60) {
        score += 25.0 * (1.0 - (fg_idle_time / 60.0));
    }
    
    // CPU usage contribution (recent high CPU suggests important work)
    float avg_cpu = calculate_average_cpu(proc);
    score += (avg_cpu / 5.0);  // CPU percentage / 5
    
    // Memory pressure adjustments
    if (memory_pressure) {
        // Under memory pressure, reduce score of high memory users
        long avg_mem = calculate_average_memory(proc);
        if (avg_mem > 500000) {  // More than ~500MB
            score -= 20.0;
        }
    }

    float normalized = score / 150.0;
    if (normalized > 1.0) normalized = 1.0;
    
    // Then map to -20 to 20 range
    float android_importance = (normalized * 40.0) - 20.0;
    
    return -1 * android_importance;
}

int change_process_priority(pid_t pid, int requested_priority) {
    // Validate requested priority is in valid range (-20 to 20)
    if (requested_priority < -20 || requested_priority > 20) {
        log_message("Invalid priority request from PID %d: %d", pid, requested_priority);
        return -1;
    }
    
    // Find the process in our tracked list
    TrackedProcess *proc = NULL;
    for (int i = 0; i < process_count; i++) {
        if (processes[i].pid == pid) {
            proc = &processes[i];
            break;
        }
    }
    
    if (!proc) {
        log_message("PID %d not found in tracked processes", pid);
        return -1;
    }
    
    // Apply a boost/reduction to the importance score
    // Store the requested priority
    proc->requested_priority = requested_priority;
    log_message("PID %d requested priority change to %d", pid, requested_priority);
    
    // Force a state update based on new priority
    update_process_state(proc, (float)requested_priority);
    
    return 0;
}

const char* get_cgroup_for_state(ProcessState state) {
    switch (state) {
        case PROCESS_STATE_FOREGROUND: return CGROUP_FOREGROUND;
        case PROCESS_STATE_VISIBLE: return CGROUP_VISIBLE;
        case PROCESS_STATE_SERVICE: return CGROUP_SERVICE;
        case PROCESS_STATE_BACKGROUND: return CGROUP_BACKGROUND;
        case PROCESS_STATE_CACHED: return CGROUP_CACHED;
        default: return CGROUP_BACKGROUND;
    }
}

int get_oom_score_for_state(ProcessState state) {
    switch (state) {
        case PROCESS_STATE_FOREGROUND: return -900;  // Least likely to be killed
        case PROCESS_STATE_VISIBLE: return -800;
        case PROCESS_STATE_SERVICE: return -500;
        case PROCESS_STATE_BACKGROUND: return 0;
        case PROCESS_STATE_CACHED: return 500;      // Most likely to be killed
        default: return 0;
    }
}

void update_process_state(TrackedProcess *proc, float importance_score) {
    ProcessState old_state = proc->state;
    
    if (proc->requested_priority != 0) {
        // Use the requested priority as a strong influence
        importance_score = (importance_score + (float)proc->requested_priority * 2.0) / 3.0;
    }
    
    // Determine new state based on importance score (now in -20 to 20 range)
    if (importance_score > 10) {
        proc->state = PROCESS_STATE_CACHED;
    } else if (importance_score > 0) {
        proc->state = PROCESS_STATE_BACKGROUND;
    } else if (importance_score > -10) {
        proc->state = PROCESS_STATE_SERVICE;
    } else if (importance_score > -15) {
        proc->state = PROCESS_STATE_VISIBLE;
    } else {
        proc->state = PROCESS_STATE_FOREGROUND;
    }
    
    // Only apply changes if state changed
    if (old_state != proc->state) {
        const char *state_names[] = {
            "FOREGROUND", "VISIBLE", "SERVICE", "BACKGROUND", "CACHED"
        };
        
        log_message("[%s] PID %d State changed : %s -> %s (score: %.1f)", 
                  proc->name, proc->pid, state_names[old_state], state_names[proc->state], importance_score);
        
        // Update cgroup assignment and OOM score
        const char *target_cgroup = get_cgroup_for_state(proc->state);
        int oom_score = get_oom_score_for_state(proc->state);
        
        if (assign_to_cgroup(target_cgroup, proc->pid) == 0) {
            strncpy(proc->cgroup_path, target_cgroup, sizeof(proc->cgroup_path)-1);
        } else {
            log_message("[%s] Failed to assign to cgroup %s", proc->name, target_cgroup);
        }
        
        set_oom_score(proc->pid, oom_score);
        proc->oom_score = oom_score;
    }
}

void adjust_resource_controls(TrackedProcess *proc) {
    char path[512];
    float avg_cpu = calculate_average_cpu(proc);
    
    // CPU shares based on state and usage patterns
    int cpu_shares = 100;  // Base value
    switch (proc->state) {
        case PROCESS_STATE_FOREGROUND:
            cpu_shares = 100;
            break;
        case PROCESS_STATE_VISIBLE:
            cpu_shares = 75;
            break;
        case PROCESS_STATE_SERVICE:
            cpu_shares = 50;
            break;
        case PROCESS_STATE_BACKGROUND:
            cpu_shares = 25;
            break;
        case PROCESS_STATE_CACHED:
            cpu_shares = 10;
            break;
    }
    
    // Adjust for intensive processes
    if (avg_cpu > 50.0) {
        cpu_shares = (int)(cpu_shares * 1.2);
    }
    
    // Set CPU shares
    snprintf(path, sizeof(path), "%s/cpu.weight", proc->cgroup_path);
    FILE *f = fopen(path, "w");
    if (f) {
        fprintf(f, "%d", cpu_shares);
        fclose(f);
    }
    
    // Set memory limits if under pressure
    if (memory_pressure && proc->state >= PROCESS_STATE_BACKGROUND) {
        long avg_mem = calculate_average_memory(proc);
        long mem_limit = avg_mem * 1024 * 1.5;  // 1.5x current usage
        
        snprintf(path, sizeof(path), "%s/memory.max", proc->cgroup_path);
        f = fopen(path, "w");
        if (f) {
            fprintf(f, "%ld", mem_limit);
            fclose(f);
        }
    } else {
        // Remove memory limits when not under pressure
        snprintf(path, sizeof(path), "%s/memory.max", proc->cgroup_path);
        f = fopen(path, "w");
        if (f) {
            fprintf(f, "-1");  // No limit
            fclose(f);
        }
    }
}

void update_lru_list() {
    // Sort processes by last_active time (most recent first)
    for (int i = 0; i < process_count; i++) {
        for (int j = i + 1; j < process_count; j++) {
            if (processes[i].last_active < processes[j].last_active) {
                TrackedProcess temp = processes[i];
                processes[i] = processes[j];
                processes[j] = temp;
            }
        }
    }
}

void monitor_all_processes() {
    time_t now = time(NULL);
    pid_t focused_pid = get_focused_window_pid();
    
    log_message("Current focused PID: %d", focused_pid);
    
    // Check system memory pressure
    memory_pressure = check_memory_pressure();
    if (memory_pressure) {
        log_message("SYSTEM: Memory pressure detected");
    }
    
    // Update process metrics and calculate importance
    for (int i = 0; i < process_count; i++) {
        // Update process resources and metrics
        update_resource_history(&processes[i]);
        
        // Calculate importance score
        float importance = calculate_importance_score(&processes[i], focused_pid);
        processes[i].importance_score = importance;
        
        // Update process state based on importance
        update_process_state(&processes[i], importance);
        
        // Adjust resource controls
        adjust_resource_controls(&processes[i]);
        
        // Debug output
        log_message("Process [%s] PID %d: Score=%.1f, State=%d, CPU=%.1f%%", 
                  processes[i].name, processes[i].pid, 
                  processes[i].importance_score, processes[i].state,
                  calculate_average_cpu(&processes[i]));
    }
    
    // Update LRU list for potential low-memory situations
    update_lru_list();
    
    // If severe memory pressure, proactively kill low-priority processes
    if (memory_pressure) {
        for (int i = 0; i < process_count; i++) {
            if (processes[i].state == PROCESS_STATE_CACHED) {
                // Only kill cached processes that haven't been active in a while
                if (now - processes[i].last_active > 300) {  // 5 minutes
                    log_message("Memory pressure: Killing cached process [%s] PID %d", 
                               processes[i].name, processes[i].pid);
                    kill(processes[i].pid, SIGTERM);
                    // Process will be removed from array in the main loop
                }
            }
        }
    }
}

void initialize_process(TrackedProcess *proc, pid_t pid, const char *initial_group) {
    memset(proc, 0, sizeof(TrackedProcess));
    proc->pid = pid;
    proc->last_active = time(NULL);
    proc->last_foreground_time = time(NULL);
    proc->requested_priority = 0;
    
    // Set initial state based on group
    if (strcmp(initial_group, "foreground") == 0) {
        proc->state = PROCESS_STATE_FOREGROUND;
        strncpy(proc->cgroup_path, CGROUP_FOREGROUND, sizeof(proc->cgroup_path)-1);
    } else {
        proc->state = PROCESS_STATE_BACKGROUND;
        strncpy(proc->cgroup_path, CGROUP_BACKGROUND, sizeof(proc->cgroup_path)-1);
    }
    
    // Get process name
    char path[256];
    snprintf(path, sizeof(path), "/proc/%d/comm", pid);
    FILE *f = fopen(path, "r");
    if (f) {
        if (fgets(proc->name, sizeof(proc->name), f)) {
            // Remove trailing newline
            size_t len = strlen(proc->name);
            if (len > 0 && proc->name[len-1] == '\n')
                proc->name[len-1] = '\0';
        }
        fclose(f);
    }
    
    // Get command line
    snprintf(path, sizeof(path), "/proc/%d/cmdline", pid);
    f = fopen(path, "r");
    if (f) {
        size_t bytes = fread(proc->cmdline, 1, sizeof(proc->cmdline)-1, f);
        if (bytes > 0) {
            proc->cmdline[bytes] = '\0';
            // Replace null bytes with spaces for readability
            for (size_t i = 0; i < bytes; i++) {
                if (proc->cmdline[i] == '\0') proc->cmdline[i] = ' ';
            }
        }
        fclose(f);
    }
    
    // Check if system service
    proc->is_system_service = is_system_service(pid);
    
    // Set initial OOM score
    int oom_score = get_oom_score_for_state(proc->state);
    set_oom_score(pid, oom_score);
    proc->oom_score = oom_score;
    
    log_message("Initialized process [%s] PID %d in %s", proc->name, pid, initial_group);
}

void launch_and_track_process(const char *group, char *const argv[]) {
    if (process_count >= MAX_PROCESSES) {
        log_message("Too many processes tracked.");
        return;
    }

    pid_t pid = fork();
    if (pid == -1) {
        perror("fork");
        return;
    }

    if (pid == 0) { // child
        const char *target_group = strcmp(group, "foreground") == 0 ? CGROUP_FOREGROUND : CGROUP_BACKGROUND;
        if (assign_to_cgroup(target_group, getpid()) != 0) {
            log_message("Failed to assign to cgroup.");
            exit(1);
        }
        execvp(argv[0], argv);
        perror("execvp");
        exit(1);
    } else { // parent
        initialize_process(&processes[process_count], pid, group);
        process_count++;
        log_message("Process started with PID %d", pid);
    }
}

void setup_cgroups() {
    // This function assumes cgroups are already mounted at /sys/fs/cgroup
    // Create cgroup directories if they don't exist
    const char *cgroups[] = {
        CGROUP_FOREGROUND, CGROUP_VISIBLE, CGROUP_SERVICE, 
        CGROUP_BACKGROUND, CGROUP_CACHED
    };
    
    for (int i = 0; i < 5; i++) {
        mkdir(cgroups[i], 0755);
        log_message("Created cgroup directory: %s", cgroups[i]);
    }
    
    log_message("cgroup hierarchy initialized");
}

void attach_to_existing_processes() {
    // Find running processes and attach to them
    DIR *dir = opendir("/proc");
    if (!dir) {
        perror("Could not open /proc");
        return;
    }
    
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && process_count < MAX_PROCESSES) {
        // Check if this is a process directory (numeric name)
        pid_t pid = atoi(entry->d_name);
        if (pid <= 0) continue;
        
        // Skip kernel threads and our own process
        if (pid == 1 || pid == getpid()) continue;
        
        // Check if process still exists
        char path[256];
        snprintf(path, sizeof(path), "/proc/%d", pid);
        if (access(path, F_OK) != 0) continue;
        
        // Initialize the process
        initialize_process(&processes[process_count], pid, "background");
        process_count++;
    }
    
    closedir(dir);
    log_message("Attached to %d existing processes", process_count);
}

bool are_processes_related(pid_t pid1, pid_t pid2) {
    // Check if pid1 is parent of pid2
    pid_t parent = get_parent_pid(pid2);
    if (parent == pid1) return true;
    
    // Check if pid2 is parent of pid1
    parent = get_parent_pid(pid1);
    if (parent == pid2) return true;
    
    // Check if they share same parent
    pid_t parent1 = get_parent_pid(pid1);
    pid_t parent2 = get_parent_pid(pid2);
    if (parent1 > 0 && parent1 == parent2) return true;
    
    return false;
}

bool check_ipc_connections(pid_t pid1, pid_t pid2) {
    // This is a simple implementation - in practice, this would need to be more sophisticated
    // to detect shared memory segments, pipes, sockets, etc.
    
    // Check if they're related by process hierarchy
    if (are_processes_related(pid1, pid2)) return true;
    
    // Look for TCP/UDP connections between processes
    char cmd[256];
    snprintf(cmd, sizeof(cmd), "ss -p | grep %d | grep %d", pid1, pid2);
    
    FILE *fp = popen(cmd, "r");
    if (fp) {
        char buffer[512];
        bool found = false;
        if (fgets(buffer, sizeof(buffer), fp)) {
            found = true;
        }
        pclose(fp);
        if (found) return true;
    }
    
    return false;
}

void handle_signal(int sig) {
    if (sig == SIGUSR1) {
        // Print debug information
        for (int i = 0; i < process_count; i++) {
            log_message("DEBUG: Process %d [%s] State=%d Score=%.1f LastActive=%ld",
                      processes[i].pid, processes[i].name, processes[i].state, 
                      processes[i].importance_score, processes[i].last_active);
        }
    } else if (sig == SIGTERM || sig == SIGINT) {
        log_message("Shutdown signal received, cleaning up...");
        
        // Move all processes back to default cgroup
        for (int i = 0; i < process_count; i++) {
            assign_to_cgroup("/sys/fs/cgroup", processes[i].pid);
            set_oom_score(processes[i].pid, 0);  // Reset OOM score
        }
        
        should_exit = true;
    }
}

void setup_priority_change_service() {
    // Simplified - would create named pipe for priority changes
    log_message("Priority change service started");
}

void check_priority_requests() {
    // Simplified - would check for priority change requests
}

// Main function that can be called from C++ or directly
int android_scheduler_main(int argc, char *argv[]) {
    // Register signal handlers
    signal(SIGUSR1, handle_signal);
    signal(SIGTERM, handle_signal);
    signal(SIGINT, handle_signal);
    
    log_message("Android Process Scheduler starting");
    
    // Reset global variables
    process_count = 0;
    memory_pressure = false;
    should_exit = false;
    
    // Setup cgroups and services
    setup_cgroups();
    setup_priority_change_service();
    
    if (argc < 2) {
        // No process specified, attach to all existing processes
        log_message("No processes specified, attaching to existing processes");
        attach_to_existing_processes();
    } else {
        // Launch specified processes
        char *group = argv[1];
        if (strcmp(group, "foreground") == 0 || strcmp(group, "background") == 0) {
            if (argc < 3) {
                log_message("Error: No command specified for %s group", group);
                return 1;
            }
            launch_and_track_process(group, &argv[2]);
        } else {
            log_message("Error: Invalid group '%s'. Use 'foreground' or 'background'", group);
            return 1;
        }
    }
    
    // Main monitoring loop
    log_message("Android Process Scheduler running - press Ctrl+C to exit");
    while (!should_exit) {
        // Check for exited processes
        for (int i = 0; i < process_count; i++) {
            int status;
            pid_t result = waitpid(processes[i].pid, &status, WNOHANG);
            
            if (result == processes[i].pid) {
                log_message("[%s] exited.", processes[i].name);
                // Replace with last element and decrease count
                processes[i] = processes[--process_count];
                i--;
                continue;
            } else if (result == -1 && errno == ECHILD) {
                // Process doesn't exist anymore
                log_message("[%s] no longer exists.", processes[i].name);
                processes[i] = processes[--process_count];
                i--;
                continue;
            }
            
            // Check if process still exists
            char path[256];
            snprintf(path, sizeof(path), "/proc/%d", processes[i].pid);
            if (access(path, F_OK) != 0) {
                log_message("[%s] vanished.", processes[i].name);
                processes[i] = processes[--process_count];
                i--;
                continue;
            }
        }

        check_priority_requests();
        
        // Monitor and adjust all tracked processes
        monitor_all_processes();
        
        // Sleep for monitoring interval
        sleep(MONITOR_INTERVAL);
    }
    
    log_message("Android Process Scheduler shutting down");
    return 0;
} 