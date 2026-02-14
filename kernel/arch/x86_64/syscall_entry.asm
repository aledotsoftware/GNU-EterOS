; =============================================================================
; éterOS - Syscall Entry Point (x86_64)
; =============================================================================

[bits 64]

global syscall_entry
extern syscall_handler
extern kernel_stack_top

section .bss
align 16
user_rsp_scratch: resq 1

section .text

; -----------------------------------------------------------------------------
; syscall_entry
; -----------------------------------------------------------------------------
; Entry point for SYSCALL instruction.
; Hardware state on entry:
;   RIP = loaded from MSR_LSTAR (this address)
;   CS  = loaded from MSR_STAR 47:32 (0x08)
;   SS  = loaded from MSR_STAR 47:32 + 8 (0x10)
;   RCX = Saved User RIP
;   R11 = Saved User RFLAGS
;   RSP = Unchanged (User Stack)
;   Interrupts disabled (IF=0) due to MSR_SFMASK
; -----------------------------------------------------------------------------
syscall_entry:
    ; 1. Save User RSP to scratch variable (safe because IF=0)
    mov [rel user_rsp_scratch], rsp

    ; 2. Load Kernel RSP
    mov rsp, [rel kernel_stack_top]

    ; 3. Push User RSP to Kernel Stack (to restore later)
    push qword [rel user_rsp_scratch]

    ; 4. Push registers to match struct syscall_regs layout
    ; struct syscall_regs {
    ;     uint64_t r11;    /* Offset 0 */
    ;     uint64_t rcx;    /* Offset 8 */
    ;     uint64_t rbp;    /* Offset 16 */
    ;     uint64_t rdi;    /* Offset 24 */
    ;     uint64_t rsi;    /* Offset 32 */
    ;     uint64_t rdx;    /* Offset 40 */
    ;     uint64_t r10;    /* Offset 48 */
    ;     uint64_t r8;     /* Offset 56 */
    ;     uint64_t r9;     /* Offset 64 */
    ;     uint64_t rax;    /* Offset 72 */
    ; };

    push rax    ; Offset 72
    push r9     ; Offset 64
    push r8     ; Offset 56
    push r10    ; Offset 48
    push rdx    ; Offset 40
    push rsi    ; Offset 32
    push rdi    ; Offset 24
    push rbp    ; Offset 16
    push rcx    ; Offset 8  (Saved RIP)
    push r11    ; Offset 0  (Saved RFLAGS)

    ; 5. Prepare argument for handler (struct pointer)
    mov rdi, rsp

    ; 6. Align stack if necessary (RSP should be 16-byte aligned before call)
    ; We pushed 1 (user rsp) + 10 (regs) = 11 qwords.
    ; If kernel_stack_top was 16-byte aligned, RSP is now (11*8) = 88 bytes off.
    ; 88 is divisible by 8 but not 16. So we are NOT aligned.
    ; We need to push one more dummy value or adjust.
    ; Let's push a dummy qword to align.

    sub rsp, 8  ; Alignment padding

    ; 7. Enable interrupts (standard syscall behavior)
    sti

    ; 8. Call C Handler
    call syscall_handler

    ; 9. Disable interrupts
    cli

    add rsp, 8  ; Remove alignment padding

    ; 10. Restore registers
    pop r11
    pop rcx
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop r10
    pop r8
    pop r9
    pop rax

    ; 11. Restore User RSP
    pop rsp

    ; 12. Return to User Mode
    ; RCX = User RIP
    ; R11 = User RFLAGS
    o64 sysret
