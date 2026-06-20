ENTRY(_start)

PHDRS
{
    interp PT_INTERP FLAGS(4); /* R */
    text PT_LOAD FLAGS(5); /* R E */
    data PT_LOAD FLAGS(6); /* R W */
}

SECTIONS
{
    . = 0x200000000;

    .interp : {
        *(.interp)
    } :text :interp

    .text : {
        *(.text)
    } :text

    .rodata : {
        *(.rodata)
    } :text

    .data : {
        *(.data)
    } :data

    .bss : {
        *(.bss)
    } :data

    .note.GNU-stack : {
        *(.note.GNU-stack)
    }
}
