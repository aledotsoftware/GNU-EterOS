#!/bin/bash
set -e

echo "Compiling the kernel..."
make clean
make -j$(nproc)

echo "Booting in QEMU..."
# Start QEMU headlessly and capture serial output
qemu-system-x86_64 -m 128 -drive format=raw,file=build/eteros.img,index=0,if=ide,media=disk -serial file:serial.log -nographic -no-reboot -no-shutdown &
QEMU_PID=$!

# Give it 10 seconds to boot
sleep 10

# Kill QEMU
kill $QEMU_PID 2>/dev/null || true

# Verify output
if grep -q "Version" serial.log; then
    if grep -q -E "PANIC|FAULT|ERROR" serial.log; then
        echo "Test FAILED: Boot encountered errors or panics."
        cat serial.log
        rm serial.log
        exit 1
    else
        echo "Test PASSED: Kernel booted successfully without panics."
        rm serial.log
        exit 0
    fi
else
    echo "Test FAILED: Boot banner not found."
    cat serial.log
    rm serial.log
    exit 1
fi
