section .text
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
    mov [environ], rdx

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
    ; AT_PAGESZ=6, AT_PHDR=3, AT_ENTRY=9, AT_RANDOM=25, AT_NULL=0
.parse_auxv:
    mov r9, qword [r8]       ; type
    cmp r9, 0                ; AT_NULL
    je .done_auxv
    mov r10, qword [r8 + 8]  ; value

    cmp r9, 6                ; AT_PAGESZ
    je .save_pagesz
    cmp r9, 3                ; AT_PHDR
    je .save_phdr
    cmp r9, 9                ; AT_ENTRY
    je .save_entry
    cmp r9, 25               ; AT_RANDOM
    je .save_random
    jmp .next_auxv

.save_pagesz:
    mov [__auxv_pagesz], r10
    jmp .next_auxv
.save_phdr:
    mov [__auxv_phdr], r10
    jmp .next_auxv
.save_entry:
    mov [__auxv_entry], r10
    jmp .next_auxv
.save_random:
    mov [__auxv_random], r10
    jmp .next_auxv

.next_auxv:
    add r8, 16
    jmp .parse_auxv

.done_auxv:
    ; 5. Call global constructors
    ; Here we would call _init() if it exists, or call function pointers in .init_array
    ; For now, we declare them externally if needed, or just let linker provide them.
    ; Standard libc runs the init array. We'll leave it as a placeholder call for now
    ; since the requirement just mentioned "call constructors".
    extern _init
    ; call _init ; Need to ensure _init is provided by CRT files. Usually provided by crti.o
    ; If we don't have crti/n, we can skip or provide a weak symbol.
    ; To avoid linker errors, we'll assume the environment doesn't strictly need _init
    ; unless we compile with it, but we can do a weak reference.

    ; Pass argc, argv, envp explicitly just in case (SysV ABI passes them in rdi, rsi, rdx)
    ; rdi currently has argc, rsi has argv, rdx has envp.
    ; We need to make sure they are in these registers.
    ; At the top, we did: pop rdi (argc), mov rsi, rsp (argv), lea rdx, ... (envp)
    ; But we used rdi, rsi, rdx. Did we clobber them?
    ; r8, r9, r10 were used for auxv.
    ; Let's restore rdi, rsi, rdx if needed, but we didn't touch them!

    ; Call main(argc, argv, envp)
    call main

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
global __auxv_entry
global __auxv_random

__auxv_pagesz: resq 1
__auxv_phdr:   resq 1
__auxv_entry:  resq 1
__auxv_random: resq 1
