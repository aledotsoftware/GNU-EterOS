#!/bin/bash
set -e

echo "Building kernel..."
make -j4 > /dev/null

echo "Running QEMU boot test (128M)..."
qemu-system-x86_64 -m 128 -drive format=raw,file=build/eteros.img,index=0,if=ide,media=disk -serial file:qemu_boot.log -display none -no-reboot &
QEMU_PID=$!

sleep 5
kill $QEMU_PID 2>/dev/null || true

cat qemu_boot.log | tr -cd '\11\12\15\40-\176' > qemu_boot_clean.log

if grep -q "PANIC" qemu_boot_clean.log; then
    echo "TEST FAILED: Kernel PANIC detected."
    cat qemu_boot.log
    exit 1
fi

if grep -q "FAULT" qemu_boot_clean.log; then
    echo "TEST FAILED: Kernel FAULT detected."
    cat qemu_boot.log
    exit 1
fi

if grep -q "eterOS" qemu_boot_clean.log; then
    echo "TEST PASSED: Kernel booted successfully without PANIC or FAULT."
else
    echo "TEST FAILED: eterOS banner not found in log."
    cat qemu_boot.log
    exit 1
fi

echo "All checks passed."
