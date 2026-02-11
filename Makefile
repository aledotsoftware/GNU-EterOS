# =============================================================================
# éterOS - Makefile
# =============================================================================
# Sistema de construcción para éterOS (x86_64)
#
# Targets principales:
#   make all      - Construir todo el sistema
#   make boot     - Compilar el bootloader (Stage 1 + Stage 2)
#   make kernel   - Compilar el kernel
#   make image    - Generar la imagen de disco
#   make run      - Ejecutar en QEMU
#   make debug    - Ejecutar en QEMU con depuración GDB
#   make clean    - Limpiar archivos generados
# =============================================================================

# ---- Toolchain (Cross-Compiler) ----
# Si usas x86_64-linux-gnu-gcc, cambia estas variables:
AS      = nasm
CC      = x86_64-elf-gcc
LD      = x86_64-elf-ld
OBJCOPY = x86_64-elf-objcopy

# ---- Emulador ----
QEMU    = qemu-system-x86_64

# ---- Directorios ----
BOOT_DIR    = boot/x86_64
KERNEL_DIR  = kernel
INCLUDE_DIR = include
BUILD_DIR   = build

# ---- Flags de compilación ----
ASFLAGS = -f elf64

CFLAGS  = -ffreestanding       \
          -fno-exceptions      \
          -fno-stack-protector \
          -nostdlib             \
          -nostdinc             \
          -mno-red-zone        \
          -Wall                \
          -Wextra              \
          -O2                  \
          -I$(INCLUDE_DIR)

LDFLAGS = -T $(BOOT_DIR)/linker.ld \
          -nostdlib

# ---- Archivos fuente ----
BOOT_SRC    = $(BOOT_DIR)/boot.asm

KERNEL_SRCS = $(KERNEL_DIR)/main.c              \
              $(KERNEL_DIR)/string.c             \
              $(KERNEL_DIR)/shell.c              \
              $(KERNEL_DIR)/drivers/video/vga.c  \
              $(KERNEL_DIR)/drivers/serial/serial.c \
              $(KERNEL_DIR)/drivers/input/keyboard.c \
              $(KERNEL_DIR)/arch/x86_64/idt.c    \
              $(KERNEL_DIR)/arch/x86_64/pic.c

# ---- Archivos objeto ----
KERNEL_OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(KERNEL_SRCS))

# ---- Archivos de salida ----
BOOT_BIN    = $(BUILD_DIR)/boot.bin
KERNEL_ELF  = $(BUILD_DIR)/kernel.elf
KERNEL_BIN  = $(BUILD_DIR)/kernel.bin
OS_IMAGE    = $(BUILD_DIR)/eteros.img

# =============================================================================
# Targets
# =============================================================================

.PHONY: all boot kernel image run run-nographic debug clean dirs info

# NOTA WINDOWS:
# Si estás en Windows, tienes dos opciones:
#   1. WSL2 (recomendado): cd /mnt/p/EterOS && make run
#   2. PowerShell nativo:  .\build.ps1 -Target run

all: dirs image
	@echo ""
	@echo "================================================"
	@echo "  eterOS construido exitosamente!"
	@echo "  Imagen: $(OS_IMAGE)"
	@echo "  Ejecutar: make run"
	@echo "================================================"
	@echo ""

# ---- Crear directorios de build ----
dirs:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/drivers/video
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/drivers/serial
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/drivers/input
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/arch/x86_64

# ---- Bootloader (binario plano, incluye Stage 1 + Stage 2) ----
boot: dirs $(BOOT_BIN)

$(BOOT_BIN): $(BOOT_SRC)
	@echo "[ASM]  $<"
	$(AS) -f bin $< -o $@

# ---- Kernel ----
kernel: dirs $(KERNEL_BIN)

# Compilar archivos .c a .o
$(BUILD_DIR)/%.o: %.c
	@echo "[CC]   $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Enlazar objetos en un ELF, luego extraer binario plano
$(KERNEL_BIN): $(KERNEL_OBJS)
	@echo "[LD]   Enlazando kernel..."
	$(LD) $(LDFLAGS) -o $(KERNEL_ELF) $(KERNEL_OBJS)
	@echo "[BIN]  Extrayendo binario del kernel..."
	$(OBJCOPY) -O binary $(KERNEL_ELF) $(KERNEL_BIN)
	@echo "[INFO] Tamaño del kernel: $$(wc -c < $(KERNEL_BIN)) bytes"

# ---- Imagen de disco ----
# Layout del disco:
#   Sector 0          : Stage 1 (MBR)
#   Sectores 1-8      : Stage 2
#   Sectores 9+       : Kernel
image: boot kernel
	@echo "[IMG]  Generando imagen de disco..."
	@# Crear imagen vacía de 1.44 MB (formato floppy)
	dd if=/dev/zero of=$(OS_IMAGE) bs=512 count=2880 2>/dev/null
	@# Escribir bootloader completo (Stage 1 + Stage 2)
	dd if=$(BOOT_BIN) of=$(OS_IMAGE) bs=512 conv=notrunc 2>/dev/null
	@# Escribir kernel después del bootloader
	@# Stage 1 = 1 sector, Stage 2 = 8 sectores, kernel va en sector 9
	dd if=$(KERNEL_BIN) of=$(OS_IMAGE) bs=512 seek=9 conv=notrunc 2>/dev/null
	@echo "[IMG]  Imagen generada: $(OS_IMAGE) ($$(wc -c < $(OS_IMAGE)) bytes)"

# ---- Ejecutar en QEMU ----
run: all
	@echo "[QEMU] Iniciando eterOS..."
	$(QEMU) -drive format=raw,file=$(OS_IMAGE),if=floppy \
	        -serial stdio                                 \
	        -m 128M                                       \
	        -no-reboot                                    \
	        -no-shutdown

# ---- Ejecutar sin ventana gráfica (ideal para WSL sin X11) ----
run-nographic: all
	@echo "[QEMU] Iniciando eterOS (modo texto)..."
	$(QEMU) -drive format=raw,file=$(OS_IMAGE),if=floppy \
	        -serial mon:stdio                             \
	        -m 128M                                       \
	        -nographic                                    \
	        -no-reboot                                    \
	        -no-shutdown

# ---- Depuración con GDB ----
debug: all
	@echo "[QEMU] Iniciando eterOS en modo depuracion..."
	@echo "       Conectar con: gdb -ex 'target remote localhost:1234'"
	$(QEMU) -drive format=raw,file=$(OS_IMAGE),if=floppy \
	        -serial stdio                                 \
	        -m 128M                                       \
	        -no-reboot                                    \
	        -no-shutdown                                  \
	        -s -S

# ---- Limpiar ----
clean:
	@echo "[CLEAN] Limpiando archivos generados..."
	rm -rf $(BUILD_DIR)
	@echo "[CLEAN] Limpieza completada."

# ---- Información del build system ----
info:
	@echo ""
	@echo "=== eterOS Build System ==="
	@echo "Assembler:    $(AS)"
	@echo "Compiler:     $(CC)"
	@echo "Linker:       $(LD)"
	@echo "Emulator:     $(QEMU)"
	@echo ""
	@echo "Boot source:  $(BOOT_SRC)"
	@echo "Kernel srcs:  $(KERNEL_SRCS)"
	@echo ""
	@echo "Boot binary:  $(BOOT_BIN)"
	@echo "Kernel ELF:   $(KERNEL_ELF)"
	@echo "Kernel BIN:   $(KERNEL_BIN)"
	@echo "Disk image:   $(OS_IMAGE)"
	@echo ""
