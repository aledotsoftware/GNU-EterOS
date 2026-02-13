/*
 * éterOS - AArch64 Exception Vectors
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 */

.section ".text"
.global vectors_el1
.align 11   // Vector table must be aligned to 2KB (2^11)

vectors_el1:
    // Current EL with SP0
    .align 7
    b   sync_handler
    .align 7
    b   irq_handler
    .align 7
    b   fiq_handler
    .align 7
    b   error_handler

    // Current EL with SPx (EL1h)
    .align 7
    b   sync_handler
    .align 7
    b   irq_handler_el1
    .align 7
    b   fiq_handler
    .align 7
    b   error_handler

    // Lower EL using AArch64
    .align 7
    b   sync_handler
    .align 7
    b   irq_handler
    .align 7
    b   fiq_handler
    .align 7
    b   error_handler

    // Lower EL using AArch32
    .align 7
    b   sync_handler
    .align 7
    b   irq_handler
    .align 7
    b   fiq_handler
    .align 7
    b   error_handler

// --- Handler Stubs ---

irq_handler_el1:
    // Save all general purpose registers (x0-x30)
    sub     sp, sp, #256    // Reserve space for 32 registers * 8 bytes (actually pair store is easier)

    stp     x0, x1, [sp, #16 * 0]
    stp     x2, x3, [sp, #16 * 1]
    stp     x4, x5, [sp, #16 * 2]
    stp     x6, x7, [sp, #16 * 3]
    stp     x8, x9, [sp, #16 * 4]
    stp     x10, x11, [sp, #16 * 5]
    stp     x12, x13, [sp, #16 * 6]
    stp     x14, x15, [sp, #16 * 7]
    stp     x16, x17, [sp, #16 * 8]
    stp     x18, x19, [sp, #16 * 9]
    stp     x20, x21, [sp, #16 * 10]
    stp     x22, x23, [sp, #16 * 11]
    stp     x24, x25, [sp, #16 * 12]
    stp     x26, x27, [sp, #16 * 13]
    stp     x28, x29, [sp, #16 * 14]
    str     x30, [sp, #16 * 15]     // LR

    // Call C handler
    bl      hal_irq_handler

    // Restore registers
    ldr     x30, [sp, #16 * 15]
    ldp     x28, x29, [sp, #16 * 14]
    ldp     x26, x27, [sp, #16 * 13]
    ldp     x24, x25, [sp, #16 * 12]
    ldp     x22, x23, [sp, #16 * 11]
    ldp     x20, x21, [sp, #16 * 10]
    ldp     x18, x19, [sp, #16 * 9]
    ldp     x16, x17, [sp, #16 * 8]
    ldp     x14, x15, [sp, #16 * 7]
    ldp     x12, x13, [sp, #16 * 6]
    ldp     x10, x11, [sp, #16 * 5]
    ldp     x8, x9, [sp, #16 * 4]
    ldp     x6, x7, [sp, #16 * 3]
    ldp     x4, x5, [sp, #16 * 2]
    ldp     x2, x3, [sp, #16 * 1]
    ldp     x0, x1, [sp, #16 * 0]

    add     sp, sp, #256
    eret

sync_handler:
    // Infinite loop for now (Panic)
    b       .

irq_handler:
    b       .

fiq_handler:
    b       .

error_handler:
    b       .
