[BITS 64]
global syscall_entry
extern syscall_handler

; Struct offsets in per_cpu_t (include/cpu.h)
; uint64_t kernel_stack; // offset 0
; uint64_t user_stack;   // offset 8

section .text
syscall_entry:
    ; 1. Swap GS to get access to per-cpu data (stored in KERNEL_GS_BASE)
    swapgs

    ; 2. Save User Stack Pointer temporarily in per-cpu struct
    ; GS:[8] = user_stack
    mov [gs:8], rsp

    ; 3. Load Kernel Stack Pointer from per-cpu struct
    ; RSP = GS:[0]
    mov rsp, [gs:0]

    ; 4. Save Registers (System V AMD64 ABI + Caller Saved)
    ; RCX = Return RIP (saved by syscall)
    ; R11 = Return RFLAGS (saved by syscall)

    push rcx    ; Save User RIP
    push r11    ; Save User RFLAGS
    push rbp    ; Save Base Pointer
    push rbx    ; Save Callee-saved
    push r12
    push r13
    push r14
    push r15

    ; 5. Call Handler
    ; Handler signature: void syscall_handler(uint64_t syscall_number, args...)
    ; C Args: RDI, RSI, RDX, RCX, R8, R9
    ; Syscall Args (Linux/standard): RAX (num), RDI, RSI, RDX, R10, R8, R9

    ; Mapping:
    ; Handler Arg 1 (RDI) = RAX (Syscall Num)
    ; Handler Arg 2 (RSI) = RDI (Arg 1)
    ; Handler Arg 3 (RDX) = RSI (Arg 2)
    ; Handler Arg 4 (RCX) = RDX (Arg 3)

    mov rcx, rdx ; 4th arg
    mov rdx, rsi ; 3rd arg
    mov rsi, rdi ; 2nd arg
    mov rdi, rax ; 1st arg

    ; Align stack if necessary (RSP should be 16-byte aligned before call)
    ; Pushed 8 regs (8*8=64 bytes). If we started aligned, we are aligned.

    cld         ; Clear direction flag (C ABI requirement)
    call syscall_handler

    ; 6. Restore Registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbx
    pop rbp
    pop r11     ; Restore RFLAGS to R11
    pop rcx     ; Restore RIP to RCX

    ; 7. Restore User Stack
    mov rsp, [gs:8]

    ; 8. Swap GS back
    swapgs

    ; 9. Return to Ring 3
    sysretq
