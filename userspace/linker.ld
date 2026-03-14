ENTRY(_start)

PHDRS
{
    text PT_LOAD FLAGS(5); /* R E */
    data PT_LOAD FLAGS(6); /* R W */
}

SECTIONS
{
    . = 0x200000000;

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
}
