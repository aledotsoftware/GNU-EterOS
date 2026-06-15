global _start

extern main
extern exit
extern __run_atexit
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
    mov [rel environ], rdx

    ; 4. Find auxv
    ; envp is an array of pointers (null-terminated).
    ; rdx currently points to envp[0].
    ; Loop until we find a NULL pointer.
    mov r8, rdx
.find_auxv:
    cmp qword [r8], 0
    je .found_envp_end
    add r8, 8
    jmp .find_auxv
.found_envp_end:
    add r8, 8        ; Skip the NULL pointer after envp
    ; Now r8 points to the start of auxv

    ; At this point, r8 points to the start of auxv.
    ; Parse auxv: each entry is 16 bytes (type in qword, value in qword).
    ; AT_PHDR=3, AT_PHENT=4, AT_PHNUM=5, AT_PAGESZ=6, AT_BASE=7, AT_ENTRY=9, AT_RANDOM=25, AT_NULL=0
.parse_auxv:
    mov r9, qword [r8]       ; type
    cmp r9, 0                ; AT_NULL
    je .done_auxv
    mov r10, qword [r8 + 8]  ; value

    cmp r9, 6                ; AT_PAGESZ
    je .save_pagesz
    cmp r9, 3                ; AT_PHDR
    je .save_phdr
    cmp r9, 4                ; AT_PHENT
    je .save_phent
    cmp r9, 5                ; AT_PHNUM
    je .save_phnum
    cmp r9, 9                ; AT_ENTRY
    je .save_entry
    cmp r9, 7                ; AT_BASE
    je .save_base
    cmp r9, 25               ; AT_RANDOM
    je .save_random
    jmp .next_auxv

.save_pagesz:
    mov [rel __auxv_pagesz], r10
    jmp .next_auxv
.save_phdr:
    mov [rel __auxv_phdr], r10
    jmp .next_auxv
.save_phent:
    mov [rel __auxv_phent], r10
    jmp .next_auxv
.save_phnum:
    mov [rel __auxv_phnum], r10
    jmp .next_auxv
.save_entry:
    mov [rel __auxv_entry], r10
    jmp .next_auxv
.save_base:
    mov [rel __auxv_base], r10
    jmp .next_auxv
.save_random:
    mov [rel __auxv_random], r10
    jmp .next_auxv

.next_auxv:
    add r8, 16
    jmp .parse_auxv

.done_auxv:
    ; Let's just make sure we align stack perfectly.
    mov rbp, rsp
    and rsp, ~15
    call main
    mov rsp, rbp





    ; 6. Run atexit handlers and exit
    mov r12, rax     ; Save main's return value
    call __run_atexit
    mov rdi, r12     ; Pass main's return value to exit
    call exit

    ; Should never reach here
    hlt

section .bss
global __auxv_pagesz
global __auxv_phdr
global __auxv_phent
global __auxv_phnum
global __auxv_entry
global __auxv_base
global __auxv_random

__auxv_pagesz: resq 1
__auxv_phdr:   resq 1
__auxv_phent:  resq 1
__auxv_phnum:  resq 1
__auxv_entry:  resq 1
__auxv_base:   resq 1
__auxv_random: resq 1

%ifidn __OUTPUT_FORMAT__, elf64
section .note.GNU-stack noalloc noexec nowrite progbits
%endif

global _start

extern main
extern exit
extern __run_atexit
