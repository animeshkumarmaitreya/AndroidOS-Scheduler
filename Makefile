# OS Scheduler Makefile
# Builds modular OS Scheduler system

CXX = g++
CXXFLAGS = -std=c++11 -Wall -Wextra -pedantic
LDFLAGS = -lX11

# Target executable
TARGET = os_scheduler_menu

# Source files
SRCS = menu.cpp simulator_wrapper.cpp scheduler_impl.cpp android_wrapper.cpp android_module.cpp
OBJS = $(SRCS:.cpp=.o)

# Main target
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^ $(LDFLAGS)

# Individual object files
menu.o: menu.cpp menu.h scheduler.h android_scheduler.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

simulator_wrapper.o: simulator_wrapper.cpp scheduler.h scheduler_types.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

scheduler_impl.o: scheduler_impl.cpp scheduler.h scheduler_types.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

android_wrapper.o: android_wrapper.cpp android_scheduler.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

android_module.o: android_module.cpp android_scheduler.h
	$(CXX) $(CXXFLAGS) -c $< -o $@

# Clean up
clean:
	rm -f $(OBJS) $(TARGET)

# Run the menu system
run: $(TARGET)
	./$(TARGET)

.PHONY: all clean run 