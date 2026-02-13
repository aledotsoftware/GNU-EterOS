; =============================================================================
; éterOS - x86_64 Interrupt Service Routines (ISRs) Stubs
; =============================================================================

[bits 64]

global isr_stub_timer
global isr_stub_keyboard
global isr_stub_serial

extern irq_timer_handler    ; Renombraremos las funciones en C para evitar conflictos
extern irq_keyboard_handler
extern irq_serial_handler

section .text

; -----------------------------------------------------------------------------
; Macros para salvar/restaurar contexto
; -----------------------------------------------------------------------------
%macro PUSH_ALL 0
    push rax
    push rbx
    push rcx
    push rdx
    push rsi
    push rdi
    push rbp
    push r8
    push r9
    push r10
    push r11
    push r12
    push r13
    push r14
    push r15
%endmacro

%macro POP_ALL 0
    pop r15
    pop r14
    pop r13
    pop r12
    pop r11
    pop r10
    pop r9
    pop r8
    pop rbp
    pop rdi
    pop rsi
    pop rdx
    pop rcx
    pop rbx
    pop rax
%endmacro

; -----------------------------------------------------------------------------
; ISR Timer (IRQ0)
; -----------------------------------------------------------------------------
isr_stub_timer:
    ; Hardware pushed: SS, RSP, RFLAGS, CS, RIP (5 qwords)
    PUSH_ALL
    
    ; ABI Setup
    cld                         ; Clear Direction Flag (System V ABI)
    
    ; call irq_timer_handler      ; Llamar lógica en C
    call irq_timer_handler
    
    ; Enviar EOI manual (PIC1_COMMAND 0x20, PIC_EOI 0x20)
    ; mov al, 0x20
    ; out 0x20, al
    
    POP_ALL
    iretq

; -----------------------------------------------------------------------------
; ISR Keyboard (IRQ1)
; -----------------------------------------------------------------------------
isr_stub_keyboard:
    PUSH_ALL
    cld
    call irq_keyboard_handler
    POP_ALL
    iretq

; -----------------------------------------------------------------------------
; ISR Serial (IRQ4)
; -----------------------------------------------------------------------------
isr_stub_serial:
    PUSH_ALL
    cld
    call irq_serial_handler
    POP_ALL
    iretq
