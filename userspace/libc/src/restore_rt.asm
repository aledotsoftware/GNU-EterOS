section .text
%ifidn __OUTPUT_FORMAT__, elf64
section .note.GNU-stack noalloc noexec nowrite progbits
%endif

section .text
global __restore_rt
__restore_rt:
    mov rax, 15
    syscall
