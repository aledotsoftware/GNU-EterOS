[BITS 64]
global syscall_entry
extern syscall_handler

%include "kernel/arch/x86_64/asm_macros.inc"

; Struct offsets in per_cpu_t (include/cpu.h)
; uint64_t kernel_stack; // offset 0
; uint64_t user_stack;   // offset 8

section .text
syscall_entry:
    ; 1. Swap GS to get access to per-cpu data (stored in KERNEL_GS_BASE = cpu_info_t*)
    swapgs

    ; 2. Save User Stack Pointer temporarily in per-cpu struct (user_stack_scratch)
    ; include/cpu.h: offsetof(cpu_info_t, user_stack_scratch) = 56 (0x38)
    mov [gs:56], rsp

    ; 3. Load Kernel Stack Pointer from per-cpu struct (kernel_stack_top)
    ; include/cpu.h: offsetof(cpu_info_t, kernel_stack_top) = 48 (0x30)
    mov rsp, [gs:48]

    ; DEBUG: Print '!' to serial (Now safe on Kernel Stack)
    push rax
    push rdx
    mov dx, 0x3F8
    mov al, '!'
    out dx, al
    pop rdx
    pop rax

    ; 4. Save All Registers (Context)
    ; Matches struct syscall_regs in include/syscall.h (UPDATED)
    PUSH_ALL

    ; 5. Call Handler
    ; void syscall_handler(struct syscall_regs* regs);
    ; RDI = &regs (which is RSP)
    mov rdi, rsp
    
    cld
    call syscall_handler

    ; 6. Restore Registers
    POP_ALL

    ; 7. Restore User Stack (user_stack_scratch)
    mov rsp, [gs:56]

    ; 8. Swap GS back
    swapgs

    ; 9. Return to Ring 3
    o64 sysret
