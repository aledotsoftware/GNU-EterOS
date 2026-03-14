[bits 64]
global cpu_init_ap_wrapper
extern cpu_init_ap

section .text

cpu_init_ap_wrapper:
    ; El trampolín ya cargó RSP deseado.
    ; Asegurar alineación de 16 bytes para la ABI de System V.
    ; El push inicial de un call resta 8, así que and rsp, -16; sub rsp, 8?
    ; O simplemente:
    and rsp, -16
    
    ; EDI ya contiene el cpu_index (cargado por el trampolín)
    call cpu_init_ap
    
    ; No debería retornar
    cli
.halt:
    hlt
    jmp .halt

%ifidn __OUTPUT_FORMAT__, elf64
section .note.GNU-stack noalloc noexec nowrite progbits
%endif
