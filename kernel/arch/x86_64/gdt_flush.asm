; =============================================================================
; éterOS - GDT Flush & Load Routines
; =============================================================================

[bits 64]

global gdt_flush
global tss_flush

section .text

; -----------------------------------------------------------------------------
; void gdt_flush(uint64_t gdtr_addr)
; Carga la GDT y recarga los segmentos
; -----------------------------------------------------------------------------
gdt_flush:
    lgdt [rdi]                  ; Cargar GDTR desde el puntero pasado en RDI

    ; Recargar segmentos de datos (0x10 = Kernel Data)
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov ss, ax
    ; No recargamos GS porque GS se usa para MSR_GS_BASE en SMP y recargarlo lo pondría a cero.

    ; Recargar Code Segment (0x08) usando far return
    push 0x08                   ; Nuevo CS
    lea rax, [rel .reload_cs]   ; Dirección de retorno
    push rax
    retfq                       ; Far Return (pop RIP, pop CS)

.reload_cs:
    ret

; -----------------------------------------------------------------------------
; void tss_flush(void)
; Carga el Task Register (TR)
; -----------------------------------------------------------------------------
tss_flush:
    mov ax, 0x28                ; Offset del TSS en la GDT (Index 5 * 8 = 40 = 0x28)
    ltr ax
    ret
