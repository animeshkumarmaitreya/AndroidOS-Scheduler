#include "menu.h"
#include "scheduler.h"
#include "android_scheduler.h"
#include <iostream>
#include <string>
#include <unistd.h>

// Main menu implementation
void run_main_menu() {
    while (true) {
        // Clear screen (system dependent)
        std::cout << "\033[2J\033[1;1H";  // ANSI escape sequence to clear screen
        
        // Display menu
        std::cout << "┌─────────────────────────────────────────────────────┐" << std::endl;
        std::cout << "│               OS Scheduler Menu                    │" << std::endl;
        std::cout << "└─────────────────────────────────────────────────────┘" << std::endl;
        std::cout << std::endl;
        std::cout << "Choose a component to run:" << std::endl;
        std::cout << std::endl;
        std::cout << "1. Linux/Android Scheduler Simulator" << std::endl;
        std::cout << "   - Simulates both schedulers for educational purposes" << std::endl;
        std::cout << std::endl;
        std::cout << "2. Android Process Scheduler" << std::endl;
        std::cout << "   - Real-time process management system using cgroups" << std::endl;
        std::cout << std::endl;
        std::cout << "0. Exit" << std::endl;
        std::cout << std::endl;
        std::cout << "Enter your choice (0-2): ";
        
        std::string input;
        std::getline(std::cin, input);
        
        if (input == "1") {
            // Run Linux/Android Scheduler Simulator
            run_linux_android_simulator(0, nullptr);
        }
        else if (input == "2") {
            // Run Android Process Scheduler
            run_android_process_scheduler(0, nullptr);
        }
        else if (input == "0") {
            std::cout << "Exiting..." << std::endl;
            break;
        }
        else {
            std::cout << "Invalid choice. Press Enter to continue...";
            std::getline(std::cin, input);
        }
    }
}

// Main entry point
int main() {
    run_main_menu();
    return 0;
} 