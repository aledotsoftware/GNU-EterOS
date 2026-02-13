/*
 * éterOS - AArch64 Context Switch
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 */

.global context_switch

/*
 * void context_switch(uintptr_t* old_sp, uintptr_t new_sp);
 * x0 = old_sp (pointer to store current SP)
 * x1 = new_sp (value to load into SP)
 */
context_switch:
    // Save callee-saved registers on the current stack
    // (x19-x28, x29=FP, x30=LR)
    stp     x29, x30, [sp, #-16]!
    stp     x27, x28, [sp, #-16]!
    stp     x25, x26, [sp, #-16]!
    stp     x23, x24, [sp, #-16]!
    stp     x21, x22, [sp, #-16]!
    stp     x19, x20, [sp, #-16]!

    // Save Floating Point / SIMD registers (d8-d15 are callee-saved)
    // Optional for now, but good practice.
    stp     d14, d15, [sp, #-16]!
    stp     d12, d13, [sp, #-16]!
    stp     d10, d11, [sp, #-16]!
    stp     d8,  d9,  [sp, #-16]!

    // Save current Stack Pointer to *old_sp (x0)
    mov     x9, sp
    str     x9, [x0]

    // Load new Stack Pointer from x1
    mov     sp, x1

    // Restore registers from new stack
    ldp     d8,  d9,  [sp], #16
    ldp     d10, d11, [sp], #16
    ldp     d12, d13, [sp], #16
    ldp     d14, d15, [sp], #16

    ldp     x19, x20, [sp], #16
    ldp     x21, x22, [sp], #16
    ldp     x23, x24, [sp], #16
    ldp     x25, x26, [sp], #16
    ldp     x27, x28, [sp], #16
    ldp     x29, x30, [sp], #16

    ret
