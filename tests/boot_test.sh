#!/bin/bash
# Script to validate eterOS boot reliability

LOG_FILE="boot_test_output.log"
TIMEOUT=5

echo "Starting eterOS in QEMU for $TIMEOUT seconds..."
# Run QEMU in background, capture serial output
qemu-system-x86_64 -drive file=build/eteros.img,format=raw,index=0,media=disk \
    -serial stdio -no-reboot -display none -m 128M > $LOG_FILE 2>&1 &
QEMU_PID=$!

# Wait for boot and some initialization
sleep $TIMEOUT

# Kill QEMU
kill $QEMU_PID 2>/dev/null || true

# Analyze output
echo "Analyzing boot logs..."
FAILED=0

# Check for successful banner or reaching shell
if grep -q "Kernel loaded." $LOG_FILE; then
    echo "✅ Kernel loaded successfully."
else
    echo "❌ Kernel failed to load."
    cat $LOG_FILE
    FAILED=1
fi

if grep -qi "PANIC\|FAULT\|ERROR" $LOG_FILE; then
    echo "❌ Boot failed with errors/panics!"
    grep -i "PANIC\|FAULT\|ERROR" $LOG_FILE
    FAILED=1
else
    echo "✅ No panics or faults detected during boot."
fi

if [ $FAILED -eq 0 ]; then
    echo "Boot validation passed!"
else
    echo "Boot validation failed."
fi
