# OS Scheduler System

A modular implementation of various OS schedulers for educational and practical purposes.

## Components

The system consists of two main components:

1. **Linux/Android Scheduler Simulator** - Simulates different scheduling policies
2. **Android Process Scheduler** - A real-time process management system using cgroups

## Project Structure

The project has been organized in a modular way with header files:

- `scheduler_types.h` - Common type definitions for schedulers
- `scheduler.h` - Interface for the scheduler simulator
- `scheduler_impl.cpp` - Implementation of the scheduler simulator
- `simulator_wrapper.cpp` - Wrapper for the simulator component
- `android_scheduler.h` - Interface for the Android process scheduler
- `android_module.cpp` - Implementation of the Android process scheduler
- `android_wrapper.cpp` - C++ wrapper for the Android scheduler
- `menu.h` and `menu.cpp` - Main menu system

## Building

To build the project, simply run:

```bash
cd final
make
```

## Running

To run the application:

```bash
cd final
./os_scheduler_menu
```

Or use the provided run target:

```bash
cd final
make run
```

## Requirements

- Linux operating system
- C++11 compatible compiler
- X11 development libraries (`libx11-dev` package)
- Root/sudo privileges (for the Android Process Scheduler component)

## Installation

1. Install dependencies:
   ```
   sudo apt-get install build-essential libx11-dev
   ```

2. Build the project:
   ```
   cd final
   make
   ```

## License

This software is provided for educational purposes. 