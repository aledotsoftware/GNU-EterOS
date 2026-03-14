[BITS 64]

global user_payload_start
global user_payload_end

section .rodata

user_payload_start:
    ; 1. Magic Syscall (0xCAFEBABE)
    ; This should trigger the "Hello from Ring 3" message in the kernel log.
    mov rax, 0xCAFEBABE
    syscall

    ; 2. Exit Syscall (60 = SYS_exit)
    ; This should stop the user process.
    mov rax, 60
    syscall

    ; 3. Loop Forever (Safeguard)
    jmp $

user_payload_end:

%ifidn __OUTPUT_FORMAT__, elf64
section .note.GNU-stack noalloc noexec nowrite progbits
%endif
