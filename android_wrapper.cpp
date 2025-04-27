#include "android_scheduler.h"
#include <iostream>
#include <string>
#include <vector>
#include <sstream>
#include <cstdlib>
#include <cstring>

// The wrapper function that will be called from the menu
int run_android_process_scheduler(int argc, char *argv[]) {
    std::cout << "┌─────────────────────────────────────────────────────┐" << std::endl;
    std::cout << "│           Android Process Scheduler                │" << std::endl;
    std::cout << "│ Real-time Android-like process management system   │" << std::endl;
    std::cout << "└─────────────────────────────────────────────────────┘" << std::endl;
    std::cout << std::endl;
    
    std::cout << "USAGE:" << std::endl;
    std::cout << "  ./android_scheduler [foreground|background] program [args...]" << std::endl;
    std::cout << "  ./android_scheduler (with no arguments to monitor existing processes)" << std::endl;
    std::cout << std::endl;
    std::cout << "Examples:" << std::endl;
    std::cout << "  ./android_scheduler foreground firefox    // Launches Firefox as foreground app" << std::endl;
    std::cout << "  ./android_scheduler background sleep 100  // Launches sleep as background app" << std::endl;
    std::cout << "  ./android_scheduler                       // Monitors all existing processes" << std::endl;
    std::cout << std::endl;
    
    std::string input;
    std::cout << "Do you want to:" << std::endl;
    std::cout << "1. Attach to existing processes (monitor mode)" << std::endl;
    std::cout << "2. Launch a foreground process" << std::endl;
    std::cout << "3. Launch a background process" << std::endl;
    std::cout << "4. Return to main menu" << std::endl;
    std::cout << "Enter choice (1-4): ";
    
    std::getline(std::cin, input);
    
    if (input == "4") {
        std::cout << "Returning to main menu..." << std::endl;
        return 0;
    }
    
    std::vector<char*> args;
    args.push_back(strdup("android_scheduler"));
    
    if (input == "2" || input == "3") {
        // Add group type
        if (input == "2") {
            args.push_back(strdup("foreground"));
        } else {
            args.push_back(strdup("background"));
        }
        
        // Get command to launch
        std::cout << "Enter command to launch (e.g., firefox, gedit, sleep 100): ";
        std::string command;
        std::getline(std::cin, command);
        
        // Split command into arguments
        std::string arg;
        std::istringstream iss(command);
        while (iss >> arg) {
            args.push_back(strdup(arg.c_str()));
        }
    }
    // For option 1, no additional arguments needed
    
    // Null-terminate the args array
    args.push_back(nullptr);
    
    // Call the Android scheduler main function
    std::cout << "Starting Android scheduler..." << std::endl;
    std::cout << "Press Ctrl+C to exit when done." << std::endl;
    
    int result = android_scheduler_main(args.size() - 1, args.data());
    
    // Clean up allocated memory
    for (char* arg : args) {
        if (arg) free(arg);
    }
    
    return result;
} 