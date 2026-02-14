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

    ; 4. Save Registers to match struct syscall_regs
    ; Struct: r11, rcx, rbp, rdi, rsi, rdx, r10, r8, r9, rax
    ; Push in reverse order
    push rax    ; Syscall Number
    push r9     ; Arg 6
    push r8     ; Arg 5
    push r10    ; Arg 4 (RCX is used for RIP, R10 for Arg 4)
    push rdx    ; Arg 3
    push rsi    ; Arg 2
    push rdi    ; Arg 1
    push rbp    ; Base Pointer
    push rcx    ; RIP (Saved by syscall)
    push r11    ; RFLAGS (Saved by syscall)

    ; 5. Call Handler
    ; void syscall_handler(struct syscall_regs* regs);
    ; RDI = &regs (which is RSP)
    mov rdi, rsp
    
    cld
    call syscall_handler

    ; 6. Restore Registers
    pop r11
    pop rcx
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop r10
    pop r8
    pop r9
    pop rax     ; Restore RAX (return value? If we want to return value, we should modify it in struct)

    ; 7. Restore User Stack
    mov rsp, [gs:8]

    ; 8. Swap GS back
    swapgs

    ; 9. Return to Ring 3
    sysretq
