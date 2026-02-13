/*
 * éterOS - AArch64 Bootloader Stub
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 *
 * Handles transition from EL3/EL2 to EL1, stack setup, and jump to kmain.
 */

.section ".text.boot"

.global _start

_start:
    // Read processor ID (MPIDR_EL1)
    mrs     x0, mpidr_el1
    and     x0, x0, #3      // Get Core ID (bits 0-1)
    cbz     x0, master_core // If core 0, proceed

    // Park secondary cores
park_cpu:
    wfe
    b       park_cpu

master_core:
    // Check Current Exception Level
    mrs     x0, CurrentEL
    and     x0, x0, #12     // Get EL bits (3:2)
    cmp     x0, #12         // EL3?
    b.eq    el3_entry
    cmp     x0, #8          // EL2?
    b.eq    el2_entry
    b       el1_entry       // Already EL1

el3_entry:
    // Configure SCR_EL3 (Secure Configuration Register)
    mov     x0, #0x5B1      // RW=1 (AArch64), NS=1 (Non-secure), SMD=1 (Disable SMC)
    msr     scr_el3, x0

    // Configure SPSR_EL3 (Saved Program Status Register)
    mov     x0, #0x3c9      // EL2h, DAIF masked
    msr     spsr_el3, x0

    // Return to EL2
    adr     x0, el2_entry
    msr     elr_el3, x0
    eret

el2_entry:
    // Configure HCR_EL2 (Hypervisor Configuration Register)
    mov     x0, #0x80000000 // RW=1 (EL1 is AArch64)
    msr     hcr_el2, x0

    // Configure SPSR_EL2
    mov     x0, #0x3c5      // EL1h, DAIF masked
    msr     spsr_el2, x0

    // Return to EL1
    adr     x0, el1_entry
    msr     elr_el2, x0
    eret

el1_entry:
    // Set up stack pointer
    ldr     x0, =_stack_top
    mov     sp, x0

    // Clear BSS
    ldr     x0, =_bss_start
    ldr     x1, =_bss_end
    sub     x2, x1, x0      // Size of BSS
    cbz     x2, call_kmain  // If size is 0, skip
    mov     x3, #0

clear_bss_loop:
    str     x3, [x0], #8    // Store 0, increment address
    sub     x2, x2, #8
    cbnz    x2, clear_bss_loop

call_kmain:
    bl      kmain

    // Halt if kmain returns
halt:
    wfe
    b       halt

.section ".bss"
.align 16
.global _stack_bottom
.global _stack_top
_stack_bottom:
    .skip 65536             // 64 KB Stack
_stack_top:
