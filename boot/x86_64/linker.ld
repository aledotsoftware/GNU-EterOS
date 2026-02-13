/*
 * éterOS - Linker Script para x86_64
 *
 * Este script de enlace define la disposición de memoria del kernel.
 * El kernel se carga en la dirección 0x10000 por el bootloader (Stage 1).
 *
 * Layout de memoria:
 *   0x00000 - 0x07BFF : Reservado (IVT, BDA, Stack)
 *   0x07C00 - 0x07DFF : Stage 1 del Bootloader (MBR, 512 bytes)
 *   0x07E00 - 0x09DFF : Stage 2 del Bootloader (~4 KB)
 *   0x10000 - ...      : Kernel de éterOS
 *   0x70000 - 0x72FFF : Tablas de paginación (PML4 + PDPT + PD)
 *   0x90000            : Tope del stack
 */

ENTRY(kmain)
OUTPUT_FORMAT(elf64-x86-64)

SECTIONS
{
    /* El kernel comienza en 0x10000 */
    . = 0x10000;

    _kernel_start = .;

    /* Sección de texto (código ejecutable) */
    .text BLOCK(4K) : ALIGN(4K)
    {
        *(.text.boot)       /* Código de entrada del kernel (primero) */
        *(.text)            /* Código general */
        *(.text.*)          /* Subsecciones de código */
    }

    /* Sección de datos de solo lectura */
    .rodata BLOCK(4K) : ALIGN(4K)
    {
        *(.rodata)
        *(.rodata.*)
    }

    /* Sección de datos inicializados */
    .data BLOCK(4K) : ALIGN(4K)
    {
        *(.data)
        *(.data.*)
    }

    /* Sección BSS (datos no inicializados) */
    .bss BLOCK(4K) : ALIGN(4K)
    {
        *(COMMON)
        *(.bss)
        *(.bss.*)
    }

    /* Símbolo para marcar el final del kernel */
    _kernel_end = .;

    /* Descartar secciones innecesarias */
    /DISCARD/ :
    {
        *(.comment)
        *(.note.*)
        *(.eh_frame)
        *(.eh_frame_hdr)
    }
}
