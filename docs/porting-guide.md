# eterOS Porting Guide

This guide outlines the steps required to port eterOS to a new hardware architecture.

## Overview

eterOS is designed with a clear separation between architecture-independent code (`kernel/`, `kernel/fs/`, `kernel/net/`) and architecture-specific code (`kernel/arch/`). To port the OS, you must implement the Hardware Abstraction Layer (HAL) for your target platform.

## Step 1: Toolchain Setup

Configure a cross-compiler (e.g., GCC or Clang) targeting your specific architecture (e.g., `aarch64-elf`, `riscv64-elf`). Update the `Makefile` to conditionally select the appropriate toolchain based on an `ARCH` variable.

## Step 2: Bootloader / Initialization

Implement the entry point for your architecture. This usually involves:
- Setting up a basic stack.
- Initializing the CPU (disabling interrupts, setting up basic exception handling vectors).
- Parsing boot information (like Multiboot2, Device Tree, or ACPI) to determine available memory and hardware.
- Calling `kmain()` in `kernel/main.c`.

## Step 3: Implement the HAL

You must provide implementations for all functions defined in `include/hal.h`. Key areas include:

- **Interrupts**: `hal_interrupts_disable()`, `hal_interrupts_enable()`, and the core interrupt descriptor table (IDT/GIC) setup.
- **Memory Mapping**: `hal_mem_map()` and `hal_mem_unmap()`, interacting with the architecture's MMU/Page Tables.
- **Context Switching**: The assembly routine (e.g., `context_switch.asm`) that saves and restores CPU registers when switching tasks.
- **Syscall Entry**: The mechanism for transitioning from user-space to kernel-space (e.g., `syscall`/`sysret` on x86_64, `svc` on ARM).

## Step 4: Drivers

Implement or adapt essential drivers:
- **Timer**: A system timer (e.g., PIT, APIC Timer, ARM Generic Timer) to drive the scheduler (`timer_get_ticks()`, calling `schedule()`).
- **Serial Console**: For early debugging output.

Once these basics are in place, the core eterOS subsystems (VFS, Scheduler, PMM) should compile and run on the new platform.
