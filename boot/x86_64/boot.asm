; =============================================================================
; éterOS Bootloader - x86_64
; =============================================================================
; Bootloader de dos etapas que realiza la transición completa:
;   Real Mode (16-bit) → Protected Mode (32-bit) → Long Mode (64-bit)
;
; Disposición de memoria:
;   0x7C00 - 0x7DFF : Stage 1 (MBR, 512 bytes)
;   0x7E00 - 0x8FFF : Stage 2 (bootloader extendido, ~4 KB)
;   0x10000 - ...    : Kernel de éterOS
;
; Stage 1 carga Stage 2 + Kernel desde disco, luego salta a Stage 2.
; Stage 2 configura la GDT, paginación, Long Mode, y salta al kernel.
; =============================================================================

; #############################################################################
; STAGE 1 - Master Boot Record (512 bytes)
; #############################################################################
[bits 16]
[org 0x7C00]

; ---- Constantes ----
STAGE2_LOAD_ADDR    equ 0x7E00          ; Dirección donde se carga Stage 2
STAGE2_SECTORS      equ 16             ; Sectores de Stage 2 (8 KB)
KERNEL_LOAD_ADDR    equ 0x10000         ; Dirección donde se carga el kernel
KERNEL_SECTORS      equ 256            ; Sectores del kernel (128 KB)
STACK_TOP           equ 0x7C00          ; El stack crece hacia abajo

; =============================================================================
; Punto de entrada - Stage 1
; =============================================================================
stage1_start:
    ; Configurar segmentos y stack
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, STACK_TOP
    sti

    ; Guardar unidad de arranque (proporcionada por BIOS en DL)
    mov [boot_drive], dl

    ; Mostrar mensaje de inicio
    mov si, msg_stage1
    call print_16

    ; ---- Habilitar Línea A20 ----
    call enable_a20

    ; ---- Cargar Stage 2 desde disco ----
    mov si, msg_loading_s2
    call print_16

    mov ah, 0x02                        ; BIOS: Lectura de sectores
    mov al, STAGE2_SECTORS              ; Número de sectores
    mov ch, 0                           ; Cilindro 0
    mov cl, 2                           ; Sector 2 (Stage 2 está después del MBR)
    mov dh, 0                           ; Cabeza 0
    mov dl, [boot_drive]                ; Unidad de arranque
    mov bx, STAGE2_LOAD_ADDR            ; Buffer de destino
    int 0x13
    jc s1_disk_error

    ; ---- Cargar Kernel desde disco ----
    mov si, msg_loading_kern
    call print_16

    ; Cargar en segmento 0x1000:0x0000 = dirección lineal 0x10000
    push es
    mov ax, 0x1000
    mov es, ax
    xor bx, bx                          ; ES:BX = 0x1000:0x0000 = 0x10000

    mov ah, 0x02                        ; BIOS: Lectura de sectores
    mov al, KERNEL_SECTORS              ; Número de sectores
    mov ch, 0                           ; Cilindro 0
    mov cl, 2 + STAGE2_SECTORS          ; Sector después de Stage 2
    mov dh, 0                           ; Cabeza 0
    mov dl, [boot_drive]
    int 0x13
    pop es
    jc s1_disk_error

    mov si, msg_ok
    call print_16

    ; ---- Saltar a Stage 2 ----
    jmp 0x0000:STAGE2_LOAD_ADDR

; --- Error de disco ---
s1_disk_error:
    mov si, msg_disk_err
    call print_16
    cli
    hlt

; -----------------------------------------------------------------------------
; print_16: Imprime cadena terminada en null usando BIOS INT 10h
; -----------------------------------------------------------------------------
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

; -----------------------------------------------------------------------------
; enable_a20: Habilita la Línea A20
; -----------------------------------------------------------------------------
enable_a20:
    ; Método 1: BIOS
    mov ax, 0x2401
    int 0x15
    jnc .done

    ; Método 2: Fast A20 Gate (puerto 0x92)
    in al, 0x92
    or al, 2
    out 0x92, al

.done:
    ret

; =============================================================================
; Datos del Stage 1
; =============================================================================
msg_stage1:      db '[eterOS] Bootloader Stage 1', 13, 10, 0
msg_loading_s2:  db '  Cargando Stage 2... ', 0
msg_loading_kern:db '  Cargando Kernel... ', 0
msg_ok:          db 'OK', 13, 10, 0
msg_disk_err:    db 'ERROR DISCO!', 13, 10, 0
boot_drive:      db 0

; =============================================================================
; Firma del MBR
; =============================================================================
times 510 - ($ - $$) db 0
dw 0xAA55

; #############################################################################
; STAGE 2 - Bootloader extendido (cargado en 0x7E00)
; #############################################################################
; A partir de aquí, el código está en el sector 2+ del disco.
; Stage 2 tiene espacio suficiente para las tablas de paginación y la GDT.
; #############################################################################

stage2_start:
    mov si, s2_msg_start
    call print_16

    ; ---- Verificar soporte para Long Mode ----
    call check_long_mode

    ; ---- Detectar Mapa de Memoria (E820) ----
    call detect_memory

    ; ---- Cargar Initrd ----
    call load_initrd

    ; ---- Configurar Video (VBE) ----
    call setup_vbe

    ; ---- Información de video (Legacy - ya no se usa pero mantenemos por debug si falla VBE) ----
    ; Obtener modo de video actual (para referencia)
    mov ah, 0x0F
    int 0x10
    mov [video_mode], al

    ; ---- Transición a Modo Protegido ----
    mov si, s2_msg_pmode
    call print_16

    cli                                 ; Deshabilitar interrupciones
    lgdt [gdt32_descriptor]             ; Cargar GDT de 32 bits

    mov eax, cr0
    or eax, 1                           ; Activar bit PE
    mov cr0, eax

    jmp CODE32_SEG:protected_mode_start ; Far jump a modo protegido

; -----------------------------------------------------------------------------
; check_long_mode: Verifica soporte de CPU para modo de 64 bits
; -----------------------------------------------------------------------------
check_long_mode:
    ; Verificar CPUID extendido
    mov eax, 0x80000000
    cpuid
    cmp eax, 0x80000001
    jb .no_long_mode

    ; Verificar bit de Long Mode
    mov eax, 0x80000001
    cpuid
    test edx, (1 << 29)
    jz .no_long_mode

    mov si, s2_msg_lm_ok
    call print_16
    ret

.no_long_mode:
    mov si, s2_msg_no_lm
    call print_16
    cli
    hlt

; -----------------------------------------------------------------------------
; detect_memory: BIOS INT 0x15, EAX=0xE820
;   Guarda el mapa de memoria en la dirección 0x5000 (MEM_MAP_ADDR)
;   Formato:
;     offset 0: count (uint32) - número de entradas
;     offset 4: entry 0 (24 bytes)
;     offset 28: entry 1 ...
; -----------------------------------------------------------------------------
MEM_MAP_ADDR equ 0x5000

detect_memory:
    mov di, MEM_MAP_ADDR + 4    ; Destino para la primera entrada (dejamos 4 bytes para count)
    xor ebx, ebx                ; ebx = 0 para empezar
    xor bp, bp                  ; bp = contador de entradas
    
    mov edx, 0x534D4150         ; 'SMAP' signature
    mov eax, 0xE820
    mov ecx, 24                 ; Tamaño de entrada deseado (ACPI 3.x)
    int 0x15
    jc .error                   ; Si carry flag set, error (o función no soportada)

    cmp eax, 0x534D4150         ; Verificar firma 'SMAP'
    jne .error

    test ebx, ebx               ; Si ebx=0 y fue la primera llamada, lista vacía/error
    jz .error
    
    jmp .check_entry

.loop:
    mov edx, 0x534D4150         ; 'SMAP' (algunas BIOS corrompen registros)
    mov eax, 0xE820
    mov ecx, 24
    int 0x15
    jc .done                    ; Carry set = fin de lista

.check_entry:
    jcxz .skip_entry            ; Si length es 0, ignorar

    ; BIOS devuelve 20 bytes a veces, si es así, llenar el resto con 1 (ACPI 3.X attr)
    cmp cl, 20
    jbe .write_entry            ; Es <= 20 bytes, ok
    test byte [es:di + 20], 1   ; Valid bit for ACPI 3.X
    jz .skip_entry

.write_entry:
    inc bp                      ; Incrementar contador de entradas validas
    add di, 24                  ; Avanzar puntero destino
    
.skip_entry:
    test ebx, ebx               ; Si ebx vuelve a 0, fin
    jz .done
    
    jmp .loop

.done:
    mov [MEM_MAP_ADDR], bp      ; Guardar número de entradas al inicio (uint32) (16-bit write es seguro aqui)
    mov si, s2_msg_mem_ok
    call print_16
    ret

.error:
    mov si, s2_msg_mem_err
    call print_16
    ret

; -----------------------------------------------------------------------------
; load_initrd: Carga el Initrd desde disco
; -----------------------------------------------------------------------------
INITRD_LOAD_ADDR    equ 0x40000
INITRD_SECTORS      equ 64
INITRD_START_LBA    equ 1 + STAGE2_SECTORS + KERNEL_SECTORS

load_initrd:
    mov si, s2_msg_initrd
    call print_16

    ; Leer sectores del Initrd
    mov eax, INITRD_START_LBA       ; LBA Inicio
    mov ecx, INITRD_SECTORS         ; Cantidad de sectores
    mov edi, INITRD_LOAD_ADDR       ; Destino (Linear Addr)

    call read_sectors_lba
    ret

; -----------------------------------------------------------------------------
; read_sectors_lba: Lee sectores usando conversión LBA -> CHS
;   EAX = Start LBA
;   CX  = Count (max 64KB total read due to segment limits if not careful, but loop handles 512b steps)
;   EDI = Destination Linear Address
; -----------------------------------------------------------------------------
read_sectors_lba:
.loop:
    push eax
    push cx
    push edi

    ; LBA to CHS (Floppy 1.44MB: 18 SPT, 2 Heads)
    xor edx, edx
    mov ecx, 18
    div ecx             ; EAX = LBA / 18, EDX = LBA % 18

    inc edx             ; Sector (1-based)
    mov cl, dl          ; CL = Sector

    xor edx, edx
    mov ebx, 2
    div ebx             ; EAX = Cylinder, EDX = Head

    mov ch, al          ; CH = Cylinder
    mov dh, dl          ; DH = Head
    mov dl, [boot_drive]; DL = Drive

    ; Destination Address Conversion (Linear -> Seg:Off)
    mov ebx, [esp]      ; Get EDI (saved on stack)
    mov eax, ebx
    shr eax, 4
    mov es, ax          ; ES = EDI >> 4
    and ebx, 0xF        ; BX = EDI & 0xF

    mov ah, 0x02
    mov al, 1           ; Read 1 sector
    int 0x13
    jc .disk_error

    pop edi
    pop cx
    pop eax

    inc eax             ; Next LBA
    add edi, 512        ; Next Dest
    dec cx
    jnz .loop

    ret

.disk_error:
    mov si, msg_disk_err ; Reusamos mensaje de Stage 1
    call print_16
    cli
    hlt

; -----------------------------------------------------------------------------
; setup_vbe: Configura el modo de video VESA (1024x768x32)
;   Usa 0xA000 para BootInfo struct
;   Usa 0xB000 para buffers temporales de VBE Mode Info Block
; -----------------------------------------------------------------------------
BOOT_INFO_ADDR      equ 0xA000
VBE_MODE_INFO_ADDR  equ 0xB000
VBE_MODE            equ 0x118 | 0x4000  ; 1024x768x32 + Linear Framebuffer (bit 14)

setup_vbe:
    mov si, s2_msg_vbe_init
    call print_16

    ; 1. Obtener información del modo VBE
    mov ax, 0x4F01
    mov cx, 0x118                       ; Modo deseado (sin bit LFB para info)
    mov di, VBE_MODE_INFO_ADDR          ; Buffer destino (ES:DI)
    int 0x10
    
    cmp ax, 0x004F                      ; Verificar éxito (AL=4F, AH=00)
    jne .vbe_fail

    ; 2. Establecer modo VBE con Linear Framebuffer
    mov ax, 0x4F02
    mov bx, VBE_MODE
    int 0x10
    
    cmp ax, 0x004F
    jne .vbe_fail

    ; 3. Llenar estructura BootInfo en 0x9000 para el kernel
    ; Estructura BootInfo (matching include/boot.h):
    ;   0x00: Signature ("KBOT")
    ;   0x04: Mem Map Count
    ;   0x08: Mem Map Addr
    ;   0x10: FB Addr (64-bit)
    ;   0x18: Width
    ;   0x1C: Height
    ;   0x20: Pitch
    ;   0x24: BPP

    mov di, BOOT_INFO_ADDR
    mov dword [di], 0x544F424B          ; Signature "KBOT"
    
    ; Memoria (E820)
    mov eax, [MEM_MAP_ADDR]             ; Count guardado por detect_memory
    mov [di + 4], eax
    mov dword [di + 8], MEM_MAP_ADDR    ; Address low
    mov dword [di + 12], 0              ; Address high

    ; Video (VBE)
    mov eax, [VBE_MODE_INFO_ADDR + 40]  ; PhysBasePtr (Framebuffer address)
    mov [di + 16], eax                  ; addr low
    mov dword [di + 20], 0              ; addr high (32-bit addr for VBE 2.0)

    mov ax, [VBE_MODE_INFO_ADDR + 18]   ; XResolution
    movzx eax, ax
    mov [di + 24], eax                  ; Width

    mov ax, [VBE_MODE_INFO_ADDR + 20]   ; YResolution
    movzx eax, ax
    mov [di + 28], eax                  ; Height

    mov ax, [VBE_MODE_INFO_ADDR + 16]   ; BytesPerScanLine
    movzx eax, ax
    mov [di + 32], eax                  ; Pitch

    mov al, [VBE_MODE_INFO_ADDR + 25]   ; BitsPerPixel
    movzx eax, al
    mov [di + 36], eax                  ; BPP

    ; Initrd Info
    mov dword [di + 40], INITRD_LOAD_ADDR
    mov dword [di + 44], 0
    mov dword [di + 48], (INITRD_SECTORS * 512)

    ret

.vbe_fail:
    mov si, s2_msg_vbe_err
    call print_16
    ; Si falla, continuamos en modo texto VGA estándar (ya activo)
    ; Pero marcamos BootInfo como inválido (Signature 0) para que el kernel sepa
    mov dword [BOOT_INFO_ADDR], 0
    ret

.done:
    ret

.error:
    mov si, s2_msg_mem_err
    call print_16
    ret

; ---- Mensajes de Stage 2 (16-bit) ----
s2_msg_start:   db '[eterOS] Stage 2 activo', 13, 10, 0
s2_msg_lm_ok:   db '  Long Mode: Soportado', 13, 10, 0
s2_msg_no_lm:   db '  ERROR: CPU sin Long Mode!', 13, 10, 0
s2_msg_pmode:   db '  Entrando en Modo Protegido...', 13, 10, 0
s2_msg_mem_ok:  db '  Memoria detectada (E820)', 13, 10, 0
s2_msg_mem_err: db '  ERROR: Fallo al detectar memoria!', 13, 10, 0
s2_msg_initrd:  db '  Cargando Initrd...', 13, 10, 0
s2_msg_vbe_init:db '  Configurando VBE 1024x768...', 13, 10, 0
s2_msg_vbe_err: db '  ERROR: VBE Init Failed!', 13, 10, 0

video_mode:     db 0

; =============================================================================
; STAGE 2 - Modo Protegido (32-bit)
; =============================================================================
[bits 32]
protected_mode_start:
    ; Configurar segmentos de datos de 32 bits
    mov ax, DATA32_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x90000                    ; Stack alto para modo protegido

    ; Indicador visual en VGA: "PM" (Protected Mode) en verde
    mov word [0xB8000], 0x2F50          ; 'P' verde
    mov word [0xB8002], 0x2F4D          ; 'M' verde

    ; ---- Configurar tablas de paginación ----
    call setup_page_tables

    ; ---- Habilitar PAE ----
    mov eax, cr4
    or eax, (1 << 5)                   ; PAE bit
    mov cr4, eax

    ; ---- Cargar PML4 en CR3 ----
    mov eax, PAGE_TABLE_ADDR
    mov cr3, eax

    ; ---- Habilitar Long Mode en EFER MSR ----
    mov ecx, 0xC0000080                 ; IA32_EFER MSR
    rdmsr
    or eax, (1 << 8)                    ; LME (Long Mode Enable)
    wrmsr

    ; ---- Activar paginación ----
    mov eax, cr0
    or eax, (1 << 31)                   ; PG bit
    mov cr0, eax

    ; ---- Cargar GDT de 64 bits y saltar a Long Mode ----
    lgdt [gdt64_descriptor]
    jmp CODE64_SEG:long_mode_start

; -----------------------------------------------------------------------------
; setup_page_tables: Configura paginación con identity mapping
;   Usa páginas de 2 MB (huge pages) para mapear los primeros 8 MB
;
;   Estructura:
;     PML4[0] → PDPT (en PAGE_TABLE_ADDR + 0x1000)
;     PDPT[0] → PD   (en PAGE_TABLE_ADDR + 0x2000)
;     PD[0-3] → 4 páginas de 2 MB cada una = 8 MB
; -----------------------------------------------------------------------------
PAGE_TABLE_ADDR equ 0x70000             ; Dirección base de las tablas

setup_page_tables:
    ; 1. Limpiar 6 páginas de 4 KB (PML4 + PDPT + 4*PD = 24 KB)
    mov edi, PAGE_TABLE_ADDR
    xor eax, eax
    mov ecx, (4096 * 6) / 4            ; 24 KB / 4 bytes
    rep stosd

    ; 2. Configurar PML4[0] → PDPT
    mov eax, PAGE_TABLE_ADDR + 0x1000   ; Dirección de PDPT
    or eax, 0x03                        ; Present + Writable
    mov [PAGE_TABLE_ADDR], eax

    ; 3. Configurar PDPT[0..3] → PD0..PD3 (Mapear 4GB)
    mov edi, PAGE_TABLE_ADDR + 0x1000   ; PDPT Base
    mov eax, PAGE_TABLE_ADDR + 0x2000   ; Base del primer PD
    or eax, 0x03                        ; Present + RW
    mov ecx, 4                          ; 4 PDs para cubrir 4GB
.loop_pdpt:
    mov [edi], eax
    add edi, 8                          ; Siguiente entrada en PDPT (8 bytes)
    add eax, 0x1000                     ; Siguiente dirección de tabla PD
    loop .loop_pdpt

    ; 4. Llenar los 4 PDs con entradas de 2MB (Identity Mapping completo)
    mov edi, PAGE_TABLE_ADDR + 0x2000   ; Inicio de PD0
    mov eax, 0x00000083                 ; Phys 0 + Present + RW + Huge (2MB)
    mov ecx, 512 * 4                    ; 2048 entradas (4GB / 2MB)
    
.loop_pds:
    mov [edi], eax                      ; Escribir entrada
    add edi, 8                          ; Siguiente entrada en la tabla
    add eax, 0x200000                   ; Siguiente dirección física (+2MB)
    loop .loop_pds

    ret

; =============================================================================
; GDT de 32 bits (para la transición a Modo Protegido)
; =============================================================================
gdt32_start:
    dq 0x0000000000000000               ; Descriptor nulo

gdt32_code:
    dw 0xFFFF                           ; Limit 0-15
    dw 0x0000                           ; Base 0-15
    db 0x00                             ; Base 16-23
    db 10011010b                        ; Access: Present, Ring0, Code, Readable
    db 11001111b                        ; Flags: 4K gran, 32-bit + Limit 16-19
    db 0x00                             ; Base 24-31

gdt32_data:
    dw 0xFFFF
    dw 0x0000
    db 0x00
    db 10010010b                        ; Access: Present, Ring0, Data, Writable
    db 11001111b
    db 0x00
gdt32_end:

gdt32_descriptor:
    dw gdt32_end - gdt32_start - 1
    dd gdt32_start

CODE32_SEG equ gdt32_code - gdt32_start
DATA32_SEG equ gdt32_data - gdt32_start

; =============================================================================
; GDT de 64 bits (para Long Mode)
; =============================================================================
gdt64_start:
    dq 0x0000000000000000               ; Descriptor nulo

gdt64_code:
    dw 0x0000                           ; Limit (ignorado en 64-bit)
    dw 0x0000                           ; Base
    db 0x00                             ; Base
    db 10011010b                        ; Access: Present, Ring0, Code, Readable
    db 00100000b                        ; Flags: Long Mode
    db 0x00                             ; Base

gdt64_data:
    dw 0x0000
    dw 0x0000
    db 0x00
    db 10010010b                        ; Access: Present, Ring0, Data, Writable
    db 00000000b
    db 0x00
gdt64_end:

gdt64_descriptor:
    dw gdt64_end - gdt64_start - 1
    dq gdt64_start

CODE64_SEG equ gdt64_code - gdt64_start
DATA64_SEG equ gdt64_data - gdt64_start

; =============================================================================
; STAGE 2 - Long Mode (64-bit)
; =============================================================================
[bits 64]
long_mode_start:
    ; Configurar segmentos de 64 bits
    mov ax, DATA64_SEG
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Configurar stack de 64 bits
    mov rsp, 0x90000

    ; ---- Habilitar SSE (Requerido por GCC en x86_64) ----
    mov rax, cr0
    and ax, 0xFFFB          ; Clear EM (bit 2)
    or ax, 0x0002           ; Set MP (bit 1)
    mov cr0, rax

    mov rax, cr4
    or ax, 0x0600           ; Set OSFXSR (bit 9) and OSXMMEXCPT (bit 10)
    mov cr4, rax

    ; Indicador visual en VGA: "64" en verde brillante
    mov word [abs 0xB8004], 0x2F36          ; '6'
    mov word [abs 0xB8006], 0x2F34          ; '4'
    mov word [abs 0xB8008], 0x2F20          ; ' '
    mov word [abs 0xB800A], 0x2F4F          ; 'O'
    mov word [abs 0xB800C], 0x2F4B          ; 'K'

    ; ---- Saltar al kernel de éterOS ----
    ; El kernel fue cargado en 0x10000 por Stage 1
    mov rax, KERNEL_LOAD_ADDR
    call rax

    ; Si el kernel retorna, detener la CPU
    cli
.halt:
    hlt
    jmp .halt

; =============================================================================
; Rellenar Stage 2 hasta completar STAGE2_SECTORS sectores (4 KB)
; =============================================================================
times (STAGE2_SECTORS * 512) - ($ - stage2_start) db 0
