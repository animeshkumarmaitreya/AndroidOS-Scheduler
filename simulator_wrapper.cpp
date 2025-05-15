/**
 * Linux Scheduler Simulator
 * Educational simulation of Linux process scheduling
 */

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

#include "scheduler.h"
#include "scheduler_types.h"

// Utility functions - made static to avoid multiple definition errors
static std::string current_time_str() {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&now_time_t), "%H:%M:%S");
    return ss.str();
}

// Task class to represent processes
class Task {
public:
    int tid;                    // Task ID
    std::string name;           // Task name
    int burst_time;             // Total execution time in ms
    int remaining_time;         // Remaining execution time
    int nice_value;             // Nice value (-20 to 19)
    int arrival_time;           // Time of arrival in the system (in ms)
    int start_time;             // Time when first execution began (in ms)
    int completion_time;        // Time when execution completed (in ms)
    bool is_running;            // Is currently executing
    bool is_started;            // Has started execution
    bool is_completed;          // Has completed execution
    int dynamic_priority;       // Current dynamic priority (for time-sharing)
    
    // Scheduling policy - common for Linux
    SchedulingPolicy scheduling_policy;
    
    // Linux scheduling properties
    LinuxClass linux_class;
    int linux_priority;         // Linux priority (0-139)
    int time_slice;             // Time slice for round robin (ms)
    int time_in_slice;          // Time spent in current slice (ms)
    
    // Statistics
    int wait_time;              // Total time spent waiting
    int response_time;          // Time until first execution
    int turnaround_time;        // Total time in system
    int num_preemptions;        // Number of times preempted
    
    Task(int id, const std::string& n, int bt, int nv, int at = 0) 
        : tid(id), name(n), burst_time(bt), remaining_time(bt), nice_value(nv),
        arrival_time(at), start_time(-1), completion_time(-1),
        is_running(false), is_started(false), is_completed(false),
        dynamic_priority(0), scheduling_policy(POLICY_TIME_SHARING),
        linux_class(LINUX_FOREGROUND),
        linux_priority(0), time_slice(100), time_in_slice(0),
        wait_time(0), response_time(-1), turnaround_time(0), num_preemptions(0) {
        
        // Calculate initial priority based on nice value
        update_linux_priority();
    }
    
    void update_linux_priority() {
        // Map nice value (-20 to 19) to priority (0-139)
        // Lower nice value = higher priority
        linux_priority = 120 + nice_value;
        
        // Adjust priority based on policy
        switch (scheduling_policy) {
            case POLICY_FIFO:
            case POLICY_ROUND_ROBIN:
                // Real-time priority range: 0-99
                linux_priority = 99 - (nice_value + 20);
                break;
            case POLICY_TIME_SHARING:
                // Normal priority range: 100-139
                linux_priority = 120 + nice_value;
                break;
            case POLICY_IDLE:
                // Always lowest priority
                linux_priority = 139;
                break;
            case POLICY_DEADLINE:
                // Highest priority for deadline scheduling
                linux_priority = 0;
                break;
        }
        
        // Class adjustments
        switch (linux_class) {
            case LINUX_FOREGROUND:
                // No adjustment for foreground
                break;
            case LINUX_BACKGROUND:
                // Background processes have lower priority
                linux_priority += 5;
                break;
            case LINUX_DAEMON:
                // System daemons can have slightly higher priority
                linux_priority -= 3;
                break;
            case LINUX_EMPTY:
                // Empty processes have lowest priority
                linux_priority = 139;
                break;
        }
        
        // Ensure in valid range
        if (linux_priority < 0) linux_priority = 0;
        if (linux_priority > 139) linux_priority = 139;
    }
    
    void run(int time_ms, int current_time) {
        if (!is_started) {
            is_started = true;
            start_time = current_time;
            response_time = start_time - arrival_time;
        }
        
        is_running = true;
        int execution_time = std::min(time_ms, remaining_time);
        time_in_slice += execution_time;
        remaining_time -= execution_time;
        
        if (remaining_time <= 0) {
            is_completed = true;
            is_running = false;
            completion_time = current_time + execution_time;
            turnaround_time = completion_time - arrival_time;
        }
    }
    
    void preempt() {
        is_running = false;
        time_in_slice = 0;
    }
    
    void wait(int time_ms) {
        wait_time += time_ms;
    }
    
    std::string to_string() const {
        std::ostringstream oss;
        oss << "Task " << tid << " [" << name << "] "
            << "Nice=" << nice_value << " "
            << "BurstTime=" << burst_time << "ms "
            << "Remaining=" << remaining_time << "ms "
            << "Priority=" << linux_priority << " "
            << "Class=" << ::to_string(linux_class) << " "
            << "Policy=" << ::to_string(scheduling_policy) << " ";
        
        if (is_completed) {
            oss << "[COMPLETED]";
        } else if (is_running) {
            oss << "[RUNNING]";
        }
        
        return oss.str();
    }
    
    std::string stats_string() const {
        std::ostringstream oss;
        oss << "Task " << tid << " [" << name << "] - "
            << "Wait: " << wait_time << "ms, "
            << "Response: " << response_time << "ms, "
            << "Turnaround: " << turnaround_time << "ms, "
            << "Preemptions: " << num_preemptions;
            
        return oss.str();
    }
};

class Scheduler {
public:
    std::vector<std::shared_ptr<Task>> all_tasks;  // Made public to fix access errors
    
    virtual ~Scheduler() {}
    
    virtual void add_task(std::shared_ptr<Task> task) = 0;
    virtual std::shared_ptr<Task> get_next_task() = 0;
    virtual void tick(int time_ms) = 0;
    virtual void preempt_current_task() = 0;
    virtual void task_completed(std::shared_ptr<Task> task) = 0;
    virtual void print_queues() const = 0;
    virtual std::string get_name() const = 0;
    
    std::shared_ptr<Task> get_current_task() const {
        return current_task;
    }
    
    int get_current_time() const {
        return current_time;
    }
    
    void increment_time(int time_ms) {
        current_time += time_ms;
    }
    
protected:
    std::shared_ptr<Task> current_task = nullptr;
    int current_time = 0; // Current time in simulation (ms)
};

class LinuxScheduler : public Scheduler {
public:
    LinuxScheduler() {
        current_time = 0;
    }
    
    void add_task(std::shared_ptr<Task> task) override {
        // Add to all tasks list
        all_tasks.push_back(task);
        
        // Add to the priority queue
        priority_queue.push_back(task);
        
        // Sort the queue
        sort_queue();
        
        std::cout << "Added task to Linux scheduler: " << task->to_string() << std::endl;
    }
    
    std::shared_ptr<Task> get_next_task() override {
        // If current task is still running, continue with it
        if (current_task && current_task->is_running && !current_task->is_completed) {
            return current_task;
        }
        
        // Get the highest priority task
        if (!priority_queue.empty()) {
            auto task = priority_queue.front();
            priority_queue.erase(priority_queue.begin());
            current_task = task;
            current_task->is_running = true;
            return current_task;
        }
        
        current_task = nullptr;
        return nullptr;
    }
    
    void tick(int time_ms) override {
        // Update waiting time for all non-running tasks
        for (auto& task : all_tasks) {
            if (!task->is_completed && !task->is_running) {
                task->wait(time_ms);
            }
        }
        
        // If no current task, get one
        if (!current_task || !current_task->is_running) {
            current_task = get_next_task();
        }
        
        // Run the current task
        if (current_task && !current_task->is_completed) {
            current_task->run(time_ms, current_time);
            
            // Check if completed
            if (current_task->is_completed) {
                task_completed(current_task);
            }
            // Check if preemption needed
            else if (should_preempt()) {
                preempt_current_task();
                current_task = get_next_task();
            }
        }
        
        // Update simulation time
        increment_time(time_ms);
    }
    
    void preempt_current_task() override {
        if (current_task) {
            current_task->preempt();
            current_task->num_preemptions++;
            
            // Re-add to the queue
            priority_queue.push_back(current_task);
            
            // Re-sort the queue
            sort_queue();
            
            current_task = nullptr;
        }
    }
    
    void task_completed(std::shared_ptr<Task> task) override {
        // Save task for statistics
        save_task(task);
        
        if (current_task == task) {
            current_task = nullptr;
        }
    }
    
    void print_queues() const override {
        std::cout << "Linux Scheduler Queues:" << std::endl;
        
        // Group tasks by class
        std::map<LinuxClass, std::vector<std::shared_ptr<Task>>> class_queues;
        
        for (auto& task : all_tasks) {
            if (!task->is_completed) {
                class_queues[task->linux_class].push_back(task);
            }
        }
        
        // Print each class queue
        for (int cls = LINUX_FOREGROUND; cls <= LINUX_EMPTY; cls++) {
            LinuxClass linux_cls = static_cast<LinuxClass>(cls);
            std::cout << "  " << to_string(linux_cls) << " Queue:" << std::endl;
            
            if (class_queues[linux_cls].empty()) {
                std::cout << "    [Empty]" << std::endl;
            } else {
                for (auto& task : class_queues[linux_cls]) {
                    if (task == current_task) {
                        std::cout << "    * " << task->to_string() << std::endl;
                    } else {
                        std::cout << "      " << task->to_string() << std::endl;
                    }
                }
            }
        }
        
        // Print currently running task
        if (current_task) {
            std::cout << "Currently Running: " << current_task->to_string() << std::endl;
        } else {
            std::cout << "No task currently running." << std::endl;
        }
    }
    
    std::string get_name() const override {
        return "Linux Scheduler";
    }
    
private:
    std::vector<std::shared_ptr<Task>> priority_queue; // Single priority queue for all tasks
    
    bool should_preempt() const {
        if (!current_task) return false;
        
        // Round Robin time slice
        if (current_task->scheduling_policy == POLICY_ROUND_ROBIN) {
            if (current_task->time_in_slice >= current_task->time_slice) {
                return true;
            }
        }
        
        // Preemptive policies
        if (!priority_queue.empty()) {
            auto highest_priority_task = priority_queue.front();
            
            // Preemption by higher priority
            if (highest_priority_task->linux_priority < current_task->linux_priority) {
                return true;
            }
        }
        
        // Time sharing preemption
        if (current_task->scheduling_policy == POLICY_TIME_SHARING) {
            if (current_task->time_in_slice >= current_task->time_slice) {
                return true;
            }
        }
        
        return false;
    }
    
    void sort_queue() {
        // Sort by Linux priorities (lower value = higher priority)
        std::sort(priority_queue.begin(), priority_queue.end(),
            [](const std::shared_ptr<Task>& a, const std::shared_ptr<Task>& b) {
                // First sort by linux_priority
                if (a->linux_priority != b->linux_priority) {
                    return a->linux_priority < b->linux_priority;
                }
                
                // Then by arrival time
                return a->arrival_time < b->arrival_time;
            });
    }
    
    void save_task(std::shared_ptr<Task> task) {
        // Create directories if they don't exist
        std::string dirname = "tasks/linux/completed";
        mkdir("tasks", 0755);
        mkdir("tasks/linux", 0755);
        mkdir(dirname.c_str(), 0755);
        
        // Save task information to a file
        std::string filename = dirname + "/task_" + std::to_string(task->tid) + ".txt";
        std::ofstream file(filename);
        
        if (file.is_open()) {
            file << "Task ID: " << task->tid << std::endl;
            file << "Name: " << task->name << std::endl;
            file << "Class: " << to_string(task->linux_class) << std::endl;
            file << "Policy: " << to_string(task->scheduling_policy) << std::endl;
            file << "Arrival Time: " << task->arrival_time << std::endl;
            file << "Start Time: " << task->start_time << std::endl;
            file << "Completion Time: " << task->completion_time << std::endl;
            file << "Burst Time: " << task->burst_time << std::endl;
            file << "Wait Time: " << task->wait_time << std::endl;
            file << "Response Time: " << task->response_time << std::endl;
            file << "Turnaround Time: " << task->turnaround_time << std::endl;
            file << "Nice Value: " << task->nice_value << std::endl;
            file << "Priority: " << task->linux_priority << std::endl;
            file << "Preemptions: " << task->num_preemptions << std::endl;
            file.close();
        }
    }
};

int run_linux_android_simulator(int, char**) {
    // Create the Linux scheduler
    std::shared_ptr<LinuxScheduler> linux_scheduler = std::make_shared<LinuxScheduler>();
    
    // Current scheduler is Linux
    std::shared_ptr<Scheduler> current_scheduler = linux_scheduler;
    
    // Task creation counter
    int next_tid = 1;
    
    std::cout << "┌─────────────────────────────────────────────────────┐" << std::endl;
    std::cout << "│           Linux Scheduler Simulator                │" << std::endl;
    std::cout << "│    Educational simulation of Linux scheduling      │" << std::endl;
    std::cout << "└─────────────────────────────────────────────────────┘" << std::endl;
    std::cout << "Type 'help' for available commands" << std::endl;
    
    std::string line;
    while (true) {
        std::cout << "scheduler> ";
        std::getline(std::cin, line);
        
        std::istringstream iss(line);
        std::string command;
        iss >> command;
        
        if (command == "create") {
            std::string name;
            int burst_time, nice_value;
            std::string class_str = "fg";       // Default
            std::string policy_str = "ts";      // Default
            
            iss >> name >> burst_time >> nice_value;
            
            // Optional parameters
            if (iss) iss >> class_str;
            if (iss) iss >> policy_str;
            
            SchedulingPolicy policy = parse_scheduling_policy(policy_str);
            
            // Create a new task
            auto task = std::make_shared<Task>(next_tid++, name, burst_time, nice_value);
            task->scheduling_policy = policy;
            task->linux_class = parse_linux_class(class_str);
            
            // Add to scheduler
            linux_scheduler->add_task(task);
            
            std::cout << "Created task: " << task->to_string() << std::endl;
        }
        else if (command == "run_linux" || command == "run") {
            std::cout << "Running Linux scheduler simulation..." << std::endl;
            
            // Run until all tasks complete
            bool all_completed = false;
            while (!all_completed) {
                linux_scheduler->tick(10); // 10ms time step
                
                // Check if all tasks are completed
                all_completed = true;
                for (auto task : linux_scheduler->all_tasks) {
                    if (!task->is_completed) {
                        all_completed = false;
                        break;
                    }
                }
            }
            
            std::cout << "All tasks completed. Final state:" << std::endl;
            linux_scheduler->print_queues();
        }
        else if (command == "step") {
            int time_ms = 10; // Default
            iss >> time_ms;
            
            std::cout << "Advancing simulation by " << time_ms << "ms" << std::endl;
            current_scheduler->tick(time_ms);
        }
        else if (command == "ts") {
            std::cout << "Task list:" << std::endl;
            for (auto task : current_scheduler->all_tasks) {
                std::cout << task->to_string() << std::endl;
            }
        }
        else if (command == "status") {
            current_scheduler->print_queues();
        }
        else if (command == "stats") {
            std::cout << "Statistics for " << current_scheduler->get_name() << ":" << std::endl;
            
            // Only show stats for completed tasks
            bool has_completed = false;
            for (auto task : current_scheduler->all_tasks) {
                if (task->is_completed) {
                    std::cout << task->stats_string() << std::endl;
                    has_completed = true;
                }
            }
            
            if (!has_completed) {
                std::cout << "No completed tasks yet." << std::endl;
            }
        }
        else if (command == "help") {
            std::cout << "Available commands:" << std::endl;
            std::cout << "  create <name> <burst_time> <nice_value> [class] [policy]" << std::endl;
            std::cout << "    Creates a new task with specified parameters" << std::endl;
            std::cout << "    class: fg (foreground), bg (background), daemon, empty" << std::endl;
            std::cout << "    policy: fifo, rr (round robin), ts (time sharing), idle, deadline" << std::endl;
            std::cout << std::endl;
            std::cout << "  run, run_linux" << std::endl;
            std::cout << "    Runs the Linux simulation until all tasks complete" << std::endl;
            std::cout << std::endl;
            std::cout << "  step [n]" << std::endl;
            std::cout << "    Advances the simulation by n milliseconds (default: 10)" << std::endl;
            std::cout << std::endl;
            std::cout << "  ts" << std::endl;
            std::cout << "    Lists all tasks (similar to ps command)" << std::endl;
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
        else if (command == "exit" || command == "quit") {
            std::cout << "Exiting simulator. Returning to main menu..." << std::endl;
            break;
        }
        else if (!command.empty()) {
            std::cout << "Unknown command: " << command << std::endl;
            std::cout << "Type 'help' for available commands" << std::endl;
        }
    }
    
    return 0;
} 