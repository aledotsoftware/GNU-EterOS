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
    ; include/cpu.h: offsetof(cpu_info_t, user_stack_scratch) = 72 (0x48)
    mov [gs:72], rsp

    ; 3. Load Kernel Stack Pointer from per-cpu struct (kernel_stack_top)
    ; include/cpu.h: offsetof(cpu_info_t, kernel_stack_top) = 64 (0x40)
    ; ONLY switch if we are coming from user space. If already in kernel space (e.g. nested call or ring 0), keep rsp.
    ; We can check this by seeing if rsp is already in kernel range.
    ; But syscall instruction is only meant to be used from ring 3.
    ; In EterOS, maybe syscalls are called from kernel space too? If so, we'd corrupt the stack.
    ; Assuming it's only called from ring 3.
    mov rsp, [gs:64]


    ; 4. Save All Registers (Context)
    ; Matches struct syscall_regs in include/syscall.h (UPDATED)
    PUSH_ALL

    ; 5. Call Handler
    ; void syscall_handler(struct syscall_regs* regs);
    ; RDI = &regs (which is RSP)
    mov rdi, rsp
    
    cld
    sti
    call syscall_handler
    cli

    ; 6. Restore Registers
    POP_ALL

    ; 7. Restore User Stack (user_stack_scratch)
    mov rsp, [gs:72]

    ; 8. Swap GS back
    swapgs

    ; 9. Return to Ring 3
    o64 sysret

%ifidn __OUTPUT_FORMAT__, elf64
section .note.GNU-stack noalloc noexec nowrite progbits
%endif
