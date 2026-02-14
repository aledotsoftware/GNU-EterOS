# =============================================================================
# éterOS - Makefile
# =============================================================================
# Sistema de construcción para éterOS
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

# ---- Configuración de Arquitectura ----
ARCH ?= x86_64

# ---- Directorios ----
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
          -mno-sse             \
          -mno-sse2            \
          -mno-mmx             \
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
              $(KERNEL_DIR)/arch/x86_64/hal_impl.c \
              $(KERNEL_DIR)/arch/x86_64/idt.c    \
              $(KERNEL_DIR)/arch/x86_64/pic.c     \
              $(KERNEL_DIR)/drivers/timer/pit.c    \
              $(KERNEL_DIR)/drivers/rtc/rtc.c      \
              $(KERNEL_DIR)/mm/heap.c             \
              $(KERNEL_DIR)/apps/santitravel.c     \
              $(KERNEL_DIR)/apps/sysmon.c          \
              $(KERNEL_DIR)/drivers/net/e1000.c    \
              $(KERNEL_DIR)/net/dhcp.c             \
              $(KERNEL_DIR)/net/dhcp_parser.c      \
              $(KERNEL_DIR)/drivers/pci/pci.c      \
              $(KERNEL_DIR)/apps/gui_demo.c

# ---- Archivos objeto ----
# Mapear .c -> .o en el directorio de build
KERNEL_OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(KERNEL_SRCS))

# ---- Archivos de salida ----
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
	@echo "  eterOS construido exitosamente! (ARCH=$(ARCH))"
	@echo "  Imagen: $(OS_IMAGE)"
ifeq ($(ARCH), x86_64)
	@echo "  Ejecutar: make run"
endif
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
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/arch/xtensa
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/drivers/timer
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/drivers/rtc
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/mm
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/apps
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/drivers/net
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/net
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/drivers/pci
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/mm

# ---- Bootloader (Solo x86_64) ----
boot: dirs $(BOOT_BIN)

$(BOOT_BIN): $(BOOT_SRC)
ifeq ($(ARCH), x86_64)
	@echo "[ASM]  $<"
	$(AS) -f bin $< -o $@
else
	@echo "[SKIP] Bootloader no necesario para $(ARCH) (o no implementado)"
endif

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
image: boot kernel
ifeq ($(ARCH), x86_64)
	@echo "[IMG]  Generando imagen de disco..."
	@# Crear imagen vacía de 1.44 MB (formato floppy)
	dd if=/dev/zero of=$(OS_IMAGE) bs=512 count=2880 2>/dev/null
	@# Escribir bootloader completo (Stage 1 + Stage 2)
	dd if=$(BOOT_BIN) of=$(OS_IMAGE) bs=512 conv=notrunc 2>/dev/null
	@# Escribir kernel después del bootloader
	@# Stage 1 = 1 sector, Stage 2 = 8 sectores, kernel va en sector 9
	dd if=$(KERNEL_BIN) of=$(OS_IMAGE) bs=512 seek=9 conv=notrunc 2>/dev/null
	@echo "[IMG]  Imagen generada: $(OS_IMAGE) ($$(wc -c < $(OS_IMAGE)) bytes)"
else
	@echo "[IMG]  Generación de imagen para $(ARCH) no implementada."
endif

# ---- Ejecutar en QEMU ----
run: all
ifeq ($(ARCH), x86_64)
	@echo "[QEMU] Iniciando eterOS..."
	$(QEMU) -drive format=raw,file=$(OS_IMAGE),if=floppy \
	        -serial stdio                                 \
	        -m 128M                                       \
	        -no-reboot                                    \
	        -no-shutdown
else
	@echo "[ERR]  'make run' solo soportado para x86_64 por ahora."
endif

# ---- Ejecutar sin ventana gráfica (ideal para WSL sin X11) ----
run-nographic: all
ifeq ($(ARCH), x86_64)
	@echo "[QEMU] Iniciando eterOS (modo texto)..."
	$(QEMU) -drive format=raw,file=$(OS_IMAGE),if=floppy \
	        -serial mon:stdio                             \
	        -m 128M                                       \
	        -nographic                                    \
	        -no-reboot                                    \
	        -no-shutdown
endif

# ---- Depuración con GDB ----
debug: all
ifeq ($(ARCH), x86_64)
	@echo "[QEMU] Iniciando eterOS en modo depuracion..."
	@echo "       Conectar con: gdb -ex 'target remote localhost:1234'"
	$(QEMU) -drive format=raw,file=$(OS_IMAGE),if=floppy \
	        -serial stdio                                 \
	        -m 128M                                       \
	        -no-reboot                                    \
	        -no-shutdown                                  \
	        -s -S
endif

# ---- Limpiar ----
clean:
	@echo "[CLEAN] Limpiando archivos generados..."
	rm -rf $(BUILD_DIR)
	@echo "[CLEAN] Limpieza completada."

# ---- Información del build system ----
info:
	@echo ""
	@echo "=== eterOS Build System ==="
	@echo "Architecture: $(ARCH)"
	@echo "Assembler:    $(AS)"
	@echo "Compiler:     $(CC)"
	@echo "Linker:       $(LD)"
ifeq ($(ARCH), x86_64)
	@echo "Emulator:     $(QEMU)"
	@echo ""
	@echo "Boot source:  $(BOOT_SRC)"
	@echo "Boot binary:  $(BOOT_BIN)"
endif
	@echo "Kernel srcs:  $(KERNEL_SRCS)"
	@echo ""
	@echo "Kernel ELF:   $(KERNEL_ELF)"
	@echo "Kernel BIN:   $(KERNEL_BIN)"
	@echo "Disk image:   $(OS_IMAGE)"
	@echo ""
