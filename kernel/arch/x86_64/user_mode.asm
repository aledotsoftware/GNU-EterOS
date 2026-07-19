[BITS 64]
global enter_user_mode

; void enter_user_mode(void* entry_point, void* user_stack)
; RDI = entry_point
; RSI = user_stack

section .text
enter_user_mode:
    ; Disable interrupts
    cli

    ; Setup Segment Selectors for Ring 3
    ; User Data Selector: 0x18 | 3 = 0x1B
    ; User Code Selector: 0x20 | 3 = 0x23

    ; Swap GS MSRs FIRST to save Kernel GS Base into KERNEL_GS_BASE
    ; Before: GS_BASE = &per_cpu, KERNEL_GS_BASE = 0
    ; After:  GS_BASE = 0, KERNEL_GS_BASE = &per_cpu
    swapgs

    mov ax, 0x1B    ; User Data Segment (Index 3, RPL 3)
    mov ds, ax
    mov es, ax
    ; Do NOT load fs and gs here, as doing so clears their MSR bases on many CPUs!
    ; Even loading a NULL selector clears the base.

    ; Construct IRETQ Frame
    ; Stack order (pushes): SS, RSP, RFLAGS, CS, RIP

    push 0x1B       ; SS (User Data)
    push rsi        ; RSP (User Stack)

    ; RFLAGS
    ; IF (Interrupt Enable) = 1 (Bit 9)
    ; IOPL = 0
    ; Reserved bit 1 = 1
    pushfq
    pop rax
    or rax, 0x200   ; Enable Interrupts in User Mode
    push rax        ; RFLAGS

    push 0x23       ; CS (User Code)
    push rdi        ; RIP (Entry Point)

    ; Exit Kernel Mode
    iretq

%ifidn __OUTPUT_FORMAT__, elf64
section .note.GNU-stack noalloc noexec nowrite progbits
%endif
