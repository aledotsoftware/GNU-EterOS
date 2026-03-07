section .text
global _start

extern main
extern exit
extern environ

_start:
    ; The kernel pushes argc, argv, envp, and auxv onto the stack before jumping here.
    ; RSP points to argc.

    ; 1. Parse argc
    pop rdi          ; argc is now in rdi

    ; 2. Parse argv
    mov rsi, rsp     ; argv is the array of pointers starting at rsp

    ; 3. Calculate envp
    ; argv is an array of (argc + 1) pointers (null-terminated).
    ; rsp currently points to argv[0].
    ; rsp + (argc + 1) * 8 will point to envp.
    lea rdx, [rsp + rdi*8 + 8] ; envp

    ; Save envp in the global 'environ' variable
    mov [environ], rdx

    ; 4. Call main(argc, argv, envp)
    call main

    ; 5. Call exit(status)
    mov rdi, rax     ; Pass main's return value to exit
    call exit

    ; Should never reach here
    hlt

section .bss
global environ
environ: resq 1
