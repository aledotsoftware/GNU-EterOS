ENTRY(_start)

SECTIONS
{
    /* Raspberry Pi loads kernels at 0x80000 */
    . = 0x80000;
    _kernel_start = .;

    .text : {
        KEEP(*(.text.boot))
        *(.text)
        *(.text.*)
    }

    .rodata : {
        . = ALIGN(16);
        *(.rodata)
        *(.rodata.*)
    }

    .data : {
        . = ALIGN(16);
        *(.data)
        *(.data.*)
    }

    .bss : {
        . = ALIGN(16);
        _bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        . = ALIGN(16);
        _bss_end = .;
    }

    _kernel_end = .;

    /DISCARD/ : {
        *(.comment)
        *(.gnu*)
        *(.note*)
        *(.eh_frame*)
    }
}
