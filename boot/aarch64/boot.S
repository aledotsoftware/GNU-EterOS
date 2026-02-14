/*
 * éterOS — aarch64 Bootloader / Entry Point
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 */

.section .text.boot
.global _start

_start:
    /* 1. Solo correr en el core principal (CPU 0) */
    mrs x0, mpidr_el1
    and x0, x0, #0xFF
    cbz x0, .cpu_ok
.cpu_halt:
    wfe
    b .cpu_halt

.cpu_ok:
    /* 2. Configurar el stack */
    ldr x0, =_stack_top
    mov sp, x0

    /* 3. Limpiar BSS (TODO) */

    /* 4. Saltar a kmain */
    bl kmain

    /* 5. Si kmain retorna, halt */
.hang:
    wfe
    b .hang
