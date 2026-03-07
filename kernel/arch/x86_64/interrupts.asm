; =============================================================================
; éterOS - x86_64 Interrupt Service Routines (ISRs) Stubs
; =============================================================================

[bits 64]

global isr_stub_timer
global isr_stub_keyboard
global isr_stub_serial
global isr_stub_mouse
global isr_stub_network
global isr_stub_lapic_timer

extern irq_timer_handler    ; Renombraremos las funciones en C para evitar conflictos
extern irq_keyboard_handler
extern irq_serial_handler
extern irq_mouse_handler
extern irq_network_handler
extern lapic_timer_handler

%include "kernel/arch/x86_64/asm_macros.inc"

section .text

; -----------------------------------------------------------------------------
; ISR Timer (IRQ0)
; -----------------------------------------------------------------------------
isr_stub_timer:
    ; Hardware pushed: SS, RSP, RFLAGS, CS, RIP (5 qwords)
    
    ; Check if we came from User Mode (CS bit 0-1 = 3)
    test qword [rsp + 8], 3     ; [rsp+8] is CS in the hard frame
    jz .kernel_entry
    swapgs
.kernel_entry:

    PUSH_ALL
    cld                         ; Clear Direction Flag (System V ABI)
    call irq_timer_handler
    POP_ALL
    
    ; Check if we are returning to User Mode
    test qword [rsp + 8], 3     ; CS
    jz .kernel_exit
    swapgs
.kernel_exit:
    iretq

; -----------------------------------------------------------------------------
; ISR Keyboard (IRQ1)
; -----------------------------------------------------------------------------
isr_stub_keyboard:
    test qword [rsp + 8], 3
    jz .k1
    swapgs
.k1:
    PUSH_ALL
    cld
    call irq_keyboard_handler
    POP_ALL
    test qword [rsp + 8], 3
    jz .k2
    swapgs
.k2:
    iretq

; -----------------------------------------------------------------------------
; ISR Serial (IRQ4)
; -----------------------------------------------------------------------------
isr_stub_serial:
    test qword [rsp + 8], 3
    jz .s1
    swapgs
.s1:
    PUSH_ALL
    cld
    call irq_serial_handler
    POP_ALL
    test qword [rsp + 8], 3
    jz .s2
    swapgs
.s2:
    iretq

; -----------------------------------------------------------------------------
; ISR Mouse (IRQ12)
; -----------------------------------------------------------------------------
isr_stub_mouse:
    test qword [rsp + 8], 3
    jz .m1
    swapgs
.m1:
    PUSH_ALL
    cld
    call irq_mouse_handler
    POP_ALL
    test qword [rsp + 8], 3
    jz .m2
    swapgs
.m2:
    iretq

; -----------------------------------------------------------------------------
; ISR Network (IRQ11)
; -----------------------------------------------------------------------------
isr_stub_network:
    test qword [rsp + 8], 3
    jz .n1
    swapgs
.n1:
    PUSH_ALL
    cld
    call irq_network_handler
    POP_ALL
    test qword [rsp + 8], 3
    jz .n2
    swapgs
.n2:
    iretq

; -----------------------------------------------------------------------------
; ISR LAPIC Timer (Vector 0x40)
; -----------------------------------------------------------------------------
isr_stub_lapic_timer:
    test qword [rsp + 8], 3
    jz .lt1
    swapgs
.lt1:
    PUSH_ALL
    cld
    call lapic_timer_handler
    POP_ALL
    test qword [rsp + 8], 3
    jz .lt2
    swapgs
.lt2:
    iretq

; -----------------------------------------------------------------------------
; IPI TLB Shootdown (Vector 0xFD)
; -----------------------------------------------------------------------------
extern isr_tlb_shootdown_c
global isr_stub_tlb_shootdown
isr_stub_tlb_shootdown:
    test qword [rsp + 8], 3
    jz .ts1
    swapgs
.ts1:
    PUSH_ALL
    cld
    call isr_tlb_shootdown_c
    POP_ALL
    test qword [rsp + 8], 3
    jz .ts2
    swapgs
.ts2:
    iretq
