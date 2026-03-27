#!/bin/bash

echo "Compiling eterOS..."
make clean && make all > build_output.txt 2>&1

if [ $? -ne 0 ]; then
    echo "Compilation failed!"
    cat build_output.txt
    exit 1
fi

echo "Running QEMU boot test..."
# Run QEMU headless with serial output redirected to a file, timeout 10 seconds
timeout 30s qemu-system-x86_64 -serial file:serial.log -no-reboot -display none -m 128M -drive file=build/eteros.img,format=raw,index=0,media=disk || true

echo "Checking logs for success..."
if grep -q "Version" serial.log; then
    if grep -q "PANIC" serial.log || grep -q "FAULT" serial.log || grep -q "ERROR" serial.log; then
        echo "Boot failed: Errors found in serial log."
        cat serial.log
        exit 1
    fi
    echo "Boot Test Passed: Kernel loaded and no panics detected."
    exit 0
else
    echo "Boot failed: Kernel did not load successfully."
    cat serial.log
    exit 1
fi
