#!/bin/bash
set -e

echo "Building kernel..."
make all

run_test() {
    local ram=$1
    echo "Running Integration Test (QEMU Headless) with ${ram}MB RAM..."

    # Start QEMU in the background
    qemu-system-x86_64 -m $ram -drive file=build/eteros.img,format=raw -serial stdio -display none -no-reboot > qemu_output.log 2>&1 &
    QEMU_PID=$!

    # Wait for 30 seconds
    echo "Waiting for 30 seconds for boot to complete..."
    sleep 30

    # Kill QEMU
    kill $QEMU_PID || true

    echo "Analyzing QEMU Output for ${ram}MB RAM..."

    # Check for banner
    if grep -q "Ether-Core" qemu_output.log || grep -q "eterOS" qemu_output.log || grep -q "Starting éterOS" qemu_output.log; then
        echo "[PASS] Boot banner found."
    else
        echo "[FAIL] Boot banner not found."
        cat qemu_output.log
        exit 1
    fi

    # Check for PANIC or FAULT
    if grep -q "PANIC" qemu_output.log || grep -q "FAULT" qemu_output.log; then
        echo "[FAIL] Kernel panic or fault detected!"
        grep -C 5 "PANIC\|FAULT" qemu_output.log
        exit 1
    else
        echo "[PASS] No PANIC or FAULT detected."
    fi

    # Check for shell prompt
    if grep -q "eteros#" qemu_output.log || grep -q "sh#" qemu_output.log; then
        echo "[PASS] Shell prompt detected."
    else
        echo "[WARN] Shell prompt not definitively detected, but boot succeeded."
    fi

    echo "Integration Test with ${ram}MB RAM Passed!"
    rm qemu_output.log
}

run_test 64
run_test 128
run_test 512

echo "All Integration Tests Passed!"
exit 0
