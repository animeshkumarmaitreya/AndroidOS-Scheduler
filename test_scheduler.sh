#!/bin/bash

# Create a temp file for commands
COMMANDS_FILE=$(mktemp)

echo "================================"
echo "Testing Linux Scheduler"
echo "================================"

# Test 1: Basic Linux Task Scheduling
cat > $COMMANDS_FILE << EOF
create task1 100 0 linux fg ts
create task2 150 0 linux fg ts
create task3 80 0 linux fg ts
run_linux
exit
EOF

echo "Test 1: Basic Linux Task Scheduling"
./os_scheduler_menu 2 < $COMMANDS_FILE

# Test 2: Linux Priority Based on Nice Values
cat > $COMMANDS_FILE << EOF
create highpri 100 -10 linux fg ts
create midpri 100 0 linux fg ts
create lowpri 100 10 linux fg ts
run_linux
exit
EOF

echo "Test 2: Linux Priority Based on Nice Values"
./os_scheduler_menu 2 < $COMMANDS_FILE

echo "================================"
echo "Testing Android Scheduler"
echo "================================"

# Test 3: Basic Android Priority Classes
cat > $COMMANDS_FILE << EOF
create foreground 100 0 android fg ts
create visible 100 0 android vis ts
create service 100 0 android svc ts
create background 100 0 android bg ts
create cached 100 0 android cache ts
run_android
exit
EOF

echo "Test 3: Basic Android Priority Classes"
./os_scheduler_menu 2 < $COMMANDS_FILE

# Test 4: Android Class Preemption
cat > $COMMANDS_FILE << EOF
create background1 200 0 android bg ts
step 50
create foreground1 100 0 android fg ts
step 50
create visible1 150 0 android vis ts
run_android
exit
EOF

echo "Test 4: Android Class Preemption"
./os_scheduler_menu 2 < $COMMANDS_FILE

# Clean up
rm $COMMANDS_FILE 