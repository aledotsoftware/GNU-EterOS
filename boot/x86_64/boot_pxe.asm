; =============================================================================
; éterOS PXE Bootloader - x86_64
; =============================================================================
; Este bootloader está diseñado para ser cargado por un servidor PXE.
; A diferencia del MBR, asume que TODO el archivo ya está en memoria
; (cargado por el ROM de la tarjeta de red, ej. SiS191).
;
; El ROM carga el archivo en 0x0000:0x7C00.
; El archivo contiene: [MBR(512)] [Stage2(8K)] [Kernel(128K)] [Initrd(32K)]
; =============================================================================

[bits 16]
[org 0x7C00]

stage1_pxe_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    mov si, msg_pxe_start
    call print_16

    ; ---- 1. Mover el Kernel ----
    ; El Kernel está en el archivo en el offset 17 * 512 = 8704.
    ; Si el archivo se cargó en 0x7C00, el kernel está en 0x7C00 + 8704 = 0x9E00.
    ; Debemos moverlo a 0x10000.
    ; Como las áreas se solapan (0x9E00-0x29E00 vs 0x10000-0x30000),
    ; copiamos de atrás hacia adelante.

    mov si, msg_moving_kern
    call print_16

    mov cx, 128 * 1024 / 4      ; 128 KB en dwords
    
    ; Setup para copia inversa
    std                         ; Set direction flag (backwards)
    mov esi, 0x9E00 + 128*1024 - 4
    mov edi, 0x10000 + 128*1024 - 4
    rep movsd
    cld                         ; Clear direction flag

    ; ---- 2. Mover el Initrd ----
    ; El Initrd está en el offset (17 + 256) * 512 = 139776.
    ; En memoria: 0x7C00 + 139776 = 0x29E00.
    ; Debe ir a 0x40000.
    
    mov si, msg_moving_initrd
    call print_16

    mov cx, 32 * 1024 / 4       ; 32 KB en dwords
    mov esi, 0x29E00
    mov edi, 0x40000
    rep movsd

    mov si, msg_ready
    call print_16

    ; ---- 3. Saltar al Stage 2 ----
    ; El Stage 2 está en el offset 512.
    ; En memoria: 0x7C00 + 512 = 0x7E00.
    ; ¡Ya está en su lugar!
    jmp 0x0000:0x7E00

; --- Helper: print_16 ---
print_16:
    pusha
    mov ah, 0x0E
.loop:
    lodsb
    test al, al
    jz .done
    int 0x10
    jmp .loop
.done:
    popa
    ret

msg_pxe_start:     db 13, 10, '[eterOS] PXE Loader SiS/SOS', 13, 10, 0
msg_moving_kern:   db '  Relocalizando Kernel... ', 0
msg_moving_initrd: db '  Relocalizando Initrd... ', 0
msg_ready:         db 'OK', 13, 10, '  Saltando a Stage 2...', 13, 10, 0

; Rellenar hasta 512 bytes para mantener los offsets del resto del archivo
times 510 - ($ - $$) db 0
dw 0xAA55
