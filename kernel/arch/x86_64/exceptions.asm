[bits 64]

%include "kernel/arch/x86_64/asm_macros.inc"

extern exception_handler_c

; Macro para excepciones SIN código de error aportado por la CPU
%macro EXCEPTION_STUB 1
global exception_stub_%1
exception_stub_%1:
    push 0          ; Error code dummy
    push %1         ; Vector number
    jmp exception_common_stub
%endmacro

; Macro para excepciones CON código de error aportado por la CPU
%macro EXCEPTION_STUB_ERR 1
global exception_stub_%1
exception_stub_%1:
    ; CPU ya puso el Error code en el stack
    push %1         ; Vector number
    jmp exception_common_stub
%endmacro

section .text

exception_common_stub:
    ; Stack actual:
    ; [rsp + 0x00] : Vector
    ; [rsp + 0x08] : Error Code
    ; [rsp + 0x10] : RIP
    ; [rsp + 0x18] : CS
    ; [rsp + 0x20] : RFLAGS
    ; [rsp + 0x28] : RSP
    ; [rsp + 0x30] : SS
    
    ; Check if coming from User Mode (CS & 3)
    test qword [rsp + 0x18], 3
    jz .kernel_entry
    swapgs
.kernel_entry:

    PUSH_ALL

    ; C function signature:
    ; void exception_handler_c(struct int_regs* regs)
    mov rdi, rsp
    cld
    call exception_handler_c

    POP_ALL
    
    ; Cleanup Vector and ErrorCode (8 bytes each)
    add rsp, 16

    ; Check if returning to User Mode (CS at rsp + 8 now)
    test qword [rsp + 0x08], 3
    jz .kernel_exit
    swapgs
.kernel_exit:
    iretq

; Generar los stubs
EXCEPTION_STUB 0
EXCEPTION_STUB 1
EXCEPTION_STUB 2
EXCEPTION_STUB 3
EXCEPTION_STUB 4
EXCEPTION_STUB 5
EXCEPTION_STUB 6
EXCEPTION_STUB 7
EXCEPTION_STUB_ERR 8
EXCEPTION_STUB 9
EXCEPTION_STUB_ERR 10
EXCEPTION_STUB_ERR 11
EXCEPTION_STUB_ERR 12
EXCEPTION_STUB_ERR 13
EXCEPTION_STUB_ERR 14
EXCEPTION_STUB 15
EXCEPTION_STUB 16
EXCEPTION_STUB_ERR 17
EXCEPTION_STUB 18
EXCEPTION_STUB 19
EXCEPTION_STUB 20
EXCEPTION_STUB_ERR 21
EXCEPTION_STUB 128
