#include "scheduler.h"
#include <iostream>
#include <string>
#include <sstream>

// This function simulates the scheduler system
int run_linux_android_simulator(int, char**) {
    // In a real implementation, we would include the Simulator class here
    // or create an instance of it from the scheduler_impl.cpp

    std::cout << "┌─────────────────────────────────────────────────────┐" << std::endl;
    std::cout << "│           OS Scheduler Simulator                   │" << std::endl;
    std::cout << "│ Comparing Linux and Android OS scheduling policies │" << std::endl;
    std::cout << "└─────────────────────────────────────────────────────┘" << std::endl;
    std::cout << "Type 'help' for available commands" << std::endl;
    
    std::string line;
    while (true) {
        std::cout << "scheduler> ";
        std::getline(std::cin, line);
        
        std::istringstream iss(line);
        std::string command;
        iss >> command;
        
        if (command == "help") {
            show_help();
        }
        else if (command == "exit" || command == "quit") {
            std::cout << "Exiting simulator. Returning to main menu..." << std::endl;
            break;
        }
        else if (!command.empty()) {
            std::cout << "This is a simplified simulator wrapper." << std::endl;
            std::cout << "The full implementation would process commands like:" << std::endl;
            std::cout << "- create, run_linux, run_android, step, ts, use, status, stats" << std::endl;
            std::cout << "Type 'help' for available commands or 'exit' to quit" << std::endl;
        }
    }
    
    return 0;
} 