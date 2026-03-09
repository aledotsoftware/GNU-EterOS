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
; Si se compila con -dPXE, asume que todo el archivo ya está en memoria (ej. SiS191).
; Stage 2 configura la GDT, paginación, Long Mode, y salta al kernel.
; =============================================================================

; #############################################################################
; STAGE 1 - Master Boot Record (512 bytes)
; #############################################################################
[bits 16]
[bits 16]
[org 0x7C00]

; ---- Constantes Globales ----
%ifndef STAGE2_SECTORS
STAGE2_SECTORS      equ 16             ; Sectores de Stage 2 (8 KB default)
%endif
%define KERNEL_LOAD_ADDR TEMP_KERNEL_ADDR
%ifndef KERNEL_SECTORS
KERNEL_SECTORS      equ 512            ; Sectores del kernel (256 KB default)
%endif
STAGE2_LOAD_ADDR    equ 0x7E00          ; Dirección donde se carga Stage 2
TEMP_KERNEL_ADDR    equ 0x10000         ; Buffer temporal bajo 1MB para BIOS (movido a 64KB)
FINAL_KERNEL_ADDR   equ 0x100000        ; Dirección final en 1MB (donde BSS tiene espacio)
STACK_TOP           equ 0x8000000       ; Stack en 128MB (Lejos del heap de 96MB)

; =============================================================================
; Punto de entrada - Stage 1
; =============================================================================
start:
    jmp short stage1_start  ; (2 bytes) EB xx
    nop                     ; (1 byte)  90

; =============================================================================
; BPB (BIOS Parameter Block) - Fake FAT12 Header for Compatibility
; Some BIOSes check this to validate USB boot media.
; =============================================================================
bpb_oem_name:           db 'MSWIN4.1'   ; 8 bytes
bpb_bytes_per_sec:      dw 512
bpb_sec_per_clus:       db 1
bpb_reserved_sec:       dw 1
bpb_num_fats:           db 2
bpb_root_ent:           dw 224
bpb_tot_sec_16:         dw 0            ; 0 for large disks (using tot_sec_32)
bpb_media_desc:         db 0xF8         ; HDD
bpb_sec_per_fat:        dw 9
bpb_sec_per_trk:        dw 63           ; Standard logic geometry
bpb_num_heads:          dw 255          ; Standard logic geometry
bpb_hidden_sec:         dd 0
bpb_tot_sec_32:         dd 0            ; Filled by formatting tools if needed

; Extended BPB (FAT12/16)
bpb_drive_num:          db 0x80         ; Physical drive number (0x80 = HDD 0)
bpb_reserved:           db 0
bpb_boot_sig:           db 0x29         ; Extended boot sig
bpb_vol_id:             dd 0x12345678
bpb_vol_label:          db 'ETEROS BOOT ' ; 11 bytes
bpb_fs_type:            db 'FAT12   '     ; 8 bytes

stage1_start:
    ; Configurar segmentos y stack
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7C00
    sti

    ; Guardar unidad de arranque (proporcionada por BIOS en DL)
    mov [boot_drive], dl

    ; Mostrar mensaje de inicio
    mov si, msg_stage1
    call print_16

    ; ---- Habilitar Línea A20 ----
    call enable_a20

%ifdef PXE
    mov si, msg_pxe_reloc
    call print_16

    ; En PXE, el archivo está completo desde 0x7C00.
    ; Relocalizar Kernel: 0x9E00 -> 0x10000 (overlap, copy end-to-start)
    mov cx, KERNEL_SECTORS * 128 ; 512 / 4 = 128 dwords por sector
    std
    mov esi, 0x9E00 + (KERNEL_SECTORS * 512) - 4
    mov edi, TEMP_KERNEL_ADDR + (KERNEL_SECTORS * 512) - 4
    rep movsd
    cld

    ; Relocalizar Initrd: 0x29E00 -> 0x40000 (32 KB fijo o dinámico)
    mov cx, (64 * 512) / 4 ; 64 sectores
    mov esi, 0x29E00
    mov edi, 0x40000
    rep movsd
%else
    ; ---- Cargar Stage 2 desde disco ----
    mov si, msg_loading_s2
    call print_16

    mov ah, 0x02                        ; BIOS: Lectura de sectores
    mov al, STAGE2_SECTORS              ; Número de sectores
    mov ch, 0                           ; Cilindro 0
    mov cl, 2                           ; Sector 2 (Stage 2 está después del MBR)
    mov dh, 0                           ; Cabeza  head 0
    mov dl, [boot_drive]                ; Unidad de arranque
    mov bx, STAGE2_LOAD_ADDR            ; Buffer de destino
    int 0x13
    jc s1_disk_error
%endif

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
msg_ok:          db 'OK', 13, 10, 0
msg_disk_err:    db 'ERROR DISCO!', 13, 10, 0
msg_pxe_reloc:   db ' [PXE Reloc] ', 0
boot_drive:      db 0

; =============================================================================
; Tabla de Particiones MBR (Simulada para USB Boot)
; Offset 446 (0x1BE) - 16 bytes por entrada, 4 entradas.
; =============================================================================
times 446 - ($ - $$) db 0

; Entrada 1: Partición Activa (Bootable), Tipo 0x83 (Linux), LBA Start 1, Size grande
db 0x80             ; Status: Active
db 0x00, 0x02, 0x00 ; CHS Start (Head 0, Sector 2, Cylinder 0) - LBA 1
db 0x83             ; Type: Linux
db 0xFE, 0x3F, 0xFF ; CHS End (Head 254, Sector 63, Cylinder 1023) - Falso
dd 0x00000001       ; LBA Start: Sector 1 (justo despues del MBR)
dd 0x00010000       ; LBA Size: 65536 sectores (32MB aprox)

; Entradas 2, 3, 4: Vacías
times 16 * 3 db 0

; =============================================================================
; Firma del MBR
; =============================================================================
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

%ifndef PXE
    ; ---- Cargar Kernel ----
    call load_kernel

    ; ---- Cargar Initrd ----
    call load_initrd
%endif

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
; load_kernel: Carga el Kernel desde disco
; -----------------------------------------------------------------------------
KERNEL_START_LBA    equ 1 + STAGE2_SECTORS

load_kernel:
    mov si, s2_msg_kern
    call print_16

    ; Leer sectores del Kernel
    mov eax, KERNEL_START_LBA       ; LBA Inicio
    mov ecx, KERNEL_SECTORS         ; Cantidad de sectores
    mov edi, KERNEL_LOAD_ADDR       ; Destino (Linear Addr 0x10000)

    call read_sectors_lba
    ret

; -----------------------------------------------------------------------------
; load_initrd: Carga el Initrd desde disco
; -----------------------------------------------------------------------------
INITRD_LOAD_ADDR    equ 0x40000     ; Mover Initrd a 0x40000 (256 KB)
%ifndef INITRD_SECTORS
INITRD_SECTORS      equ 512         ; Keep the increased limit (default)
%endif
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
; Disk Address Packet (DAP) para int 0x13 ah=42h
; -----------------------------------------------------------------------------
align 4
dap_struct:
    db 0x10                     ; Tamaño del DAP
    db 0                        ; Reservado
    dw 0                        ; Cantidad de sectores (se llena dinamicamente)
    dw 0                        ; Offset de destino (se llena dinamicamente)
    dw 0                        ; Segmento de destino (se llena dinamicamente)
    dq 0                        ; LBA de inicio (se llena dinamicamente)

; -----------------------------------------------------------------------------
; read_sectors_lba: Lee sectores usando int 0x13 Extensiones LBA
;   EAX = Start LBA
;   CX  = Count
;   EDI = Destination Linear Address (Debe ser < 1MB para BIOS real mode)
; -----------------------------------------------------------------------------
read_sectors_lba:
.loop:
    push eax
    push cx
    push edi

    ; Preparar DAP
    mov dword [dap_struct + 8], eax     ; LBA Low
    mov dword [dap_struct + 12], 0      ; LBA High
    mov word [dap_struct + 2], 1        ; Leer 1 sector a la vez para máxima compatibilidad
    
    ; Convertir Linear Address a Segment:Offset
    mov ebx, edi
    mov eax, ebx
    shr eax, 4
    mov word [dap_struct + 6], ax       ; Segmento
    and ebx, 0x0F
    mov word [dap_struct + 4], bx       ; Offset

    ; Reintentos (3 veces)
    mov byte [read_retries], 3
.retry:
    mov ah, 0x42                        ; Extended Read
    mov si, dap_struct
    mov dl, [boot_drive]
    int 0x13
    jnc .success

    ; Si falla, resetear disco e intentar de nuevo
    push ax
    xor ah, ah
    mov dl, [boot_drive]
    int 0x13
    pop ax
    
    dec byte [read_retries]
    jnz .retry

    ; Si fallan todos los reintentos, error fatal
    jmp .disk_error

.success:
    pop edi
    pop cx
    pop eax

    inc eax                             ; Siguiente LBA
    add edi, 512                        ; Siguiente dirección de destino
    dec cx
    jnz .loop                           ; Continuar hasta leer todos

    ret

.disk_error:
    mov si, msg_disk_err
    call print_16
    cli
    hlt

read_retries: db 0

; -----------------------------------------------------------------------------
; setup_vbe: Configura el modo de video VESA (1024x768x32)
;   Usa 0xA000 para BootInfo struct
;   Usa 0xB000 para buffers temporales de VBE Mode Info Block
; -----------------------------------------------------------------------------
BOOT_INFO_ADDR      equ 0x9E00          ; Justo después de Stage 2
VBE_INFO_BLOCK_ADDR equ 0x0500          ; Memoria baja libre
VBE_MODE_INFO_ADDR  equ 0x0700          ; Memoria baja libre
VBE_DEFAULT_MODE    equ 0x118 | 0x4000  ; 1024x768x32 + LFB

setup_vbe:
    mov si, s2_msg_vbe_init
    call print_16

    ; 1. Buscar el mejor modo VBE disponible
    call find_best_vbe_mode
    mov bx, ax                          ; Guardar modo seleccionado en BX (incluye bit 14 LFB)

    ; 2. Obtener información del modo seleccionado (para llenar BootInfo)
    push bx
    and bx, 0x3FFF                      ; Quitar bit LFB para 0x4F01
    mov cx, bx
    mov ax, 0x4F01
    mov di, VBE_MODE_INFO_ADDR
    int 0x10
    pop bx
    
    cmp ax, 0x004F
    jne .vbe_fail

    ; 3. Establecer modo VBE
    mov ax, 0x4F02
    ; BX ya tiene el modo (con bit 0x4000 si lo pusimos en find_best_vbe_mode)
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

; -----------------------------------------------------------------------------
; find_best_vbe_mode: Busca 1920x1080 > 1280x720 > 1024x768 (32bpp)
; Retorna: AX = Modo VBE (con bit 0x4000 activado para LFB)
; -----------------------------------------------------------------------------
find_best_vbe_mode:
    push es
    push di
    push si
    push bx
    push cx
    push dx

    ; 1. Obtener VbeInfoBlock
    mov ax, 0x4F00
    mov di, VBE_INFO_BLOCK_ADDR
    mov dword [di], 0x32454256      ; "VBE2" signature hint
    int 0x10
    cmp ax, 0x004F
    jne .fallback

    ; 2. Obtener puntero a lista de modos (Segment:Offset en offset 14)
    mov fs, [VBE_INFO_BLOCK_ADDR + 16] ; Segmento
    mov si, [VBE_INFO_BLOCK_ADDR + 14] ; Offset

    ; Variables para guardar el mejor modo encontrado
    ; Usamos stack o registros fijos? Usaremos DX para guardar el mejor modo. 0 = ninguno.
    xor dx, dx

.scan_loop:
    mov ax, fs
    mov es, ax
    mov cx, [es:si]                 ; Leer modo de la lista
    cmp cx, 0xFFFF                  ; Fin de lista?
    je .end_scan

    add si, 2                       ; Avanzar puntero

    ; 3. Obtener info del modo
    push cx                         ; Guardar modo original
    mov ax, 0x4F01
    ; CX ya tiene el modo
    mov di, VBE_MODE_INFO_ADDR
    push es                         ; Guardar ES (segmento lista modos)
    xor bx, bx
    mov es, bx                      ; ES = 0 para buffer en 0xB000
    int 0x10
    pop es                          ; Restaurar ES
    pop cx                          ; Recuperar modo original

    cmp ax, 0x004F
    jne .scan_loop

    ; 4. Verificar atributos
    ; Offset 0: ModeAttributes (bit 0=Supported, bit 3=Color, bit 4=Graph, bit 7=LFB)
    mov ax, [VBE_MODE_INFO_ADDR]
    test ax, 0x0080                 ; Soporta LFB?
    jz .scan_loop
    test ax, 0x0001                 ; Soportado por hardware?
    jz .scan_loop
    test ax, 0x0010                 ; Modo gráfico?
    jz .scan_loop

    ; Verificar BPP (Offset 25)
    cmp byte [VBE_MODE_INFO_ADDR + 25], 32
    je .check_res
    cmp byte [VBE_MODE_INFO_ADDR + 25], 24
    je .check_res
    jmp .scan_loop

.check_res:
    mov ax, [VBE_MODE_INFO_ADDR + 18] ; XRes
    mov bx, [VBE_MODE_INFO_ADDR + 20] ; YRes

    ; Check 1920x1080
    cmp ax, 1920
    jne .check_720p
    cmp bx, 1080
    jne .scan_loop
    or cx, 0x4000                   ; Activar LFB
    mov dx, cx                      ; ¡Encontramos 1080p!
    jmp .end_scan                   ; Terminar búsqueda (prioridad máxima)

.check_720p:
    cmp ax, 1280
    jne .check_768p
    cmp bx, 720
    jne .scan_loop
    or cx, 0x4000
    ; Si ya tenemos uno mejor guardado, no sobreescribir (1080p salta antes)
    mov dx, cx
    jmp .scan_loop

.check_768p:
    cmp ax, 1024
    jne .scan_loop
    cmp bx, 768
    jne .scan_loop
    or cx, 0x4000
    test dx, dx                     ; Si ya tenemos uno mejor (720p), no sobreescribir
    jnz .scan_loop
    mov dx, cx                      ; Guardar como candidato
    jmp .scan_loop

.end_scan:
    test dx, dx
    jz .fallback
    mov ax, dx                      ; Retornar mejor modo

    pop dx
    pop cx
    pop bx
    pop si
    pop di
    pop es
    ret

.fallback:
    mov ax, VBE_DEFAULT_MODE        ; 1024x768 default
    pop dx
    pop cx
    pop bx
    pop si
    pop di
    pop es
    ret

; ---- Mensajes de Stage 2 (16-bit) ----
s2_msg_start:   db '[eterOS] Stage 2 activo', 13, 10, 0
s2_msg_load_kern: db '  Cargando Kernel...', 13, 10, 0
s2_msg_lm_ok:   db '  Long Mode: Soportado', 13, 10, 0
s2_msg_no_lm:   db '  ERROR: CPU sin Long Mode!', 13, 10, 0
s2_msg_pmode:   db '  Entrando en Modo Protegido...', 13, 10, 0
s2_msg_mem_ok:  db '  Memoria detectada (E820)', 13, 10, 0
s2_msg_mem_err: db '  ERROR: Fallo al detectar memoria!', 13, 10, 0
s2_msg_kern:    db '  Cargando Kernel... ', 0
s2_msg_initrd:  db '  Cargando Initrd... ', 0
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
    mov esp, 0x9F000                    ; Stack alto para modo protegido

    ; Indicador visual en VGA: "PM" (Protected Mode) en verde
    mov word [0xB8000], 0x2F50          ; 'P' verde
    mov word [0xB8002], 0x2F4D          ; 'M' verde
    
    ; ---- Relocalizar Kernel a su posición final (1MB+) ----
    ; Esto lo hacemos en Modo Protegido para superar el límite de 1MB de la BIOS
    ; y evitar colisiones de BSS con el stack/EBDA.
    mov esi, TEMP_KERNEL_ADDR
    mov edi, FINAL_KERNEL_ADDR
    mov ecx, KERNEL_SECTORS * 128       ; (512 bytes / 4 bytes por dword) * setores
    rep movsd

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
; setup_page_tables: Configura paginación con identity mapping (4 GB)
;   Usa páginas de 2 MB (huge pages) para mapear los primeros 4 GB
;
;   Estructura:
;     PML4[0] → PDPT (en PAGE_TABLE_ADDR + 0x1000)
;     PDPT[0] → PD0  (en PAGE_TABLE_ADDR + 0x2000)
;     PDPT[1] → PD1  (en PAGE_TABLE_ADDR + 0x3000)
;     PDPT[2] → PD2  (en PAGE_TABLE_ADDR + 0x4000)
;     PDPT[3] → PD3  (en PAGE_TABLE_ADDR + 0x5000)
;     Cada PD (0..3) tiene 512 entradas de 2 MB cada una = 1 GB por PD.
; -----------------------------------------------------------------------------
PAGE_TABLE_ADDR equ 0x0A000             ; Después de Stage 2 y BootInfo

setup_page_tables:
    ; 1. Limpiar 6 páginas (PML4 + PDPT + 4*PD = 24 KB)
    mov edi, PAGE_TABLE_ADDR
    xor eax, eax
    mov ecx, (4096 * 6) / 4
    rep stosd

    ; 2. Configurar PML4[0] → PDPT
    mov eax, PAGE_TABLE_ADDR + 0x1000
    or eax, 0x03                        ; Present + Writable
    mov [PAGE_TABLE_ADDR], eax

    ; 3. Configurar PDPT[0..3] → PD0..PD3 (4 entries = 4 GB)
    mov edi, PAGE_TABLE_ADDR + 0x1000
    mov eax, PAGE_TABLE_ADDR + 0x2000
    mov ecx, 4
.loop_pdpt:
    mov [edi], eax
    or dword [edi], 0x03                ; Present + RW
    add edi, 8
    add eax, 0x1000
    loop .loop_pdpt

    ; 4. Llenar los 4 PDs (0..3) con entradas Huge (2 MB)
    mov edi, PAGE_TABLE_ADDR + 0x2000
    mov eax, 0x00000083                 ; Phys 0 + Huge + RW + Present
    mov ecx, 512 * 4                    ; 2048 entradas de 2 MB = 4 GB
.loop_pds:
    mov [edi], eax
    add edi, 8
    add eax, 0x200000
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

    ; Configurar stack de 64 bits (8MB Identity Mapped)
    mov rsp, STACK_TOP

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
    ; El kernel fue reubicado a FINAL_KERNEL_ADDR (0x100000) por Stage 2
    mov rax, FINAL_KERNEL_ADDR
    call rax

    ; Si el kernel retorna, detener la CPU
    cli
.halt:
    hlt
    jmp .halt

; =============================================================================
; Rellenar Stage 2 hasta completar el tamaño total esperado (MBR + Stage 2)
; Total = 512 (MBR) + STAGE2_SECTORS * 512 (Stage 2)
; =============================================================================
times (1 + STAGE2_SECTORS) * 512 - ($ - $$) db 0
