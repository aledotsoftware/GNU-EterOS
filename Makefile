# =============================================================================
# éterOS - Makefile
# =============================================================================
# Sistema de construcción para éterOS
# Soporta: x86_64 (Default) y aarch64 (Raspberry Pi 4)
#
# Uso:
#   make                (Construye x86_64)
#   make ARCH=aarch64   (Construye AArch64)
# =============================================================================

ARCH ?= x86_64

# ---- Directorios ----
BUILD_DIR   = build
KERNEL_DIR  = kernel
INCLUDE_DIR = include

# ---- Configuración por Arquitectura ----

ifeq ($(ARCH), x86_64)
    # Toolchain
    AS      = nasm
    CC      = x86_64-elf-gcc
    LD      = x86_64-elf-ld
    OBJCOPY = x86_64-elf-objcopy
    QEMU    = qemu-system-x86_64

    # Flags
    ASFLAGS = -f elf64
    CFLAGS  = -ffreestanding -fno-exceptions -fno-stack-protector -nostdlib -nostdinc \
              -mno-red-zone -mno-sse -mno-sse2 -mno-mmx -Wall -Wextra -O2 \
              -I$(INCLUDE_DIR) -DARCH_X86_64

    LDFLAGS = -T boot/x86_64/linker.ld -nostdlib

    # Archivos Fuente (x86_64)
    BOOT_SRC        = boot/x86_64/boot.asm
    BOOT_BIN        = $(BUILD_DIR)/boot.bin

    KERNEL_SRCS     = $(KERNEL_DIR)/main.c \
                      $(KERNEL_DIR)/string.c \
                      $(KERNEL_DIR)/shell.c \
                      $(KERNEL_DIR)/drivers/video/vga.c \
                      $(KERNEL_DIR)/drivers/serial/serial.c \
                      $(KERNEL_DIR)/drivers/input/keyboard.c \
                      $(KERNEL_DIR)/arch/x86_64/idt.c \
                      $(KERNEL_DIR)/arch/x86_64/pic.c \
                      $(KERNEL_DIR)/drivers/timer/pit.c \
                      $(KERNEL_DIR)/mm/heap.c \
                      $(KERNEL_DIR)/apps/santitravel.c \
                      $(KERNEL_DIR)/apps/sysmon.c \
                      $(KERNEL_DIR)/drivers/net/e1000.c \
                      $(KERNEL_DIR)/net/dhcp.c \
                      $(KERNEL_DIR)/drivers/pci/pci.c \
                      $(KERNEL_DIR)/mm/pmm.c \
                      $(KERNEL_DIR)/arch/x86_64/hal_impl.c \
                      $(KERNEL_DIR)/task.c \
                      $(KERNEL_DIR)/apps/gui_demo.c \
                      $(KERNEL_DIR)/drivers/video/framebuffer.c \
                      $(KERNEL_DIR)/arch/x86_64/gdt.c \
                      $(KERNEL_DIR)/mm/vmm.c \
                      $(KERNEL_DIR)/drivers/video/font.c \
                      $(KERNEL_DIR)/ui/window.c \
                      $(KERNEL_DIR)/ui/primitives.c

    KERNEL_ASM_SRCS = $(KERNEL_DIR)/arch/x86_64/interrupts.asm \
                      $(KERNEL_DIR)/arch/x86_64/gdt_flush.asm \
                      $(KERNEL_DIR)/arch/x86_64/context_switch.asm

    # Output Names
    KERNEL_ELF      = $(BUILD_DIR)/kernel.elf
    KERNEL_BIN      = $(BUILD_DIR)/kernel.bin
    OS_IMAGE        = $(BUILD_DIR)/eteros.img

    # Boot Rule
    BOOT_RULE       = $(AS) -f bin $< -o $@

else ifeq ($(ARCH), aarch64)
    # Toolchain
    AS      = aarch64-linux-gnu-as
    CC      = aarch64-linux-gnu-gcc
    LD      = aarch64-linux-gnu-ld
    OBJCOPY = aarch64-linux-gnu-objcopy
    QEMU    = qemu-system-aarch64

    # Flags
    ASFLAGS = -mcpu=cortex-a72
    CFLAGS  = -ffreestanding -fno-exceptions -fno-stack-protector -nostdlib -nostdinc \
              -Wall -Wextra -O2 -mcpu=cortex-a72 \
              -I$(INCLUDE_DIR) -DARCH_AARCH64

    LDFLAGS = -T kernel/arch/aarch64/linker.ld -nostdlib

    # Archivos Fuente (AArch64)
    # Note: No VGA, No PS/2 Keyboard, No PCI/E1000 (yet)
    KERNEL_SRCS     = $(KERNEL_DIR)/main.c \
                      $(KERNEL_DIR)/string.c \
                      $(KERNEL_DIR)/shell.c \
                      $(KERNEL_DIR)/mm/heap.c \
                      $(KERNEL_DIR)/apps/santitravel.c \
                      $(KERNEL_DIR)/mm/pmm.c \
                      $(KERNEL_DIR)/arch/aarch64/hal_impl.c \
                      $(KERNEL_DIR)/task.c \
                      $(KERNEL_DIR)/drivers/video/font.c \
                      $(KERNEL_DIR)/drivers/video/framebuffer.c \
                      $(KERNEL_DIR)/ui/window.c \
                      $(KERNEL_DIR)/ui/primitives.c \
                      $(KERNEL_DIR)/apps/gui_demo.c \
                      $(KERNEL_DIR)/arch/aarch64/stubs.c

    KERNEL_ASM_SRCS = $(KERNEL_DIR)/arch/aarch64/boot.S \
                      $(KERNEL_DIR)/arch/aarch64/context_switch.S \
                      $(KERNEL_DIR)/arch/aarch64/vectors.S

    # Output Names
    KERNEL_ELF      = $(BUILD_DIR)/kernel8.elf
    KERNEL_BIN      = $(BUILD_DIR)/kernel8.img
    OS_IMAGE        = $(BUILD_DIR)/eteros_rpi4.img

    # Boot Rule (Not used for AArch64 usually, kernel8.img is loaded directly)
    BOOT_BIN        =

endif

# ---- Objetos ----
KERNEL_OBJS     = $(patsubst %.c,$(BUILD_DIR)/%.o,$(KERNEL_SRCS))

# ASM objects extension handling
ifeq ($(ARCH), x86_64)
    KERNEL_ASM_OBJS = $(patsubst %.asm,$(BUILD_DIR)/%.o,$(KERNEL_ASM_SRCS))
else
    KERNEL_ASM_OBJS = $(patsubst %.S,$(BUILD_DIR)/%.o,$(KERNEL_ASM_SRCS))
endif

# =============================================================================
# Targets
# =============================================================================

.PHONY: all boot kernel image run run-nographic debug clean dirs info

all: dirs image
	@echo "Build complete for $(ARCH)"

dirs:
	@mkdir -p $(BUILD_DIR)
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/drivers/video
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/drivers/serial
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/drivers/input
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/arch/x86_64
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/arch/aarch64
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/drivers/timer
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/mm
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/apps
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/drivers/net
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/net
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/drivers/pci
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/ui

boot: dirs $(BOOT_BIN)

# Only for x86_64
$(BOOT_BIN): $(BOOT_SRC)
ifeq ($(ARCH), x86_64)
	@echo "[ASM]  $<"
	$(BOOT_RULE)
endif

kernel: dirs $(KERNEL_BIN)

# Compile C
$(BUILD_DIR)/%.o: %.c
	@echo "[CC]   $<"
	$(CC) $(CFLAGS) -c $< -o $@

# Compile ASM (x86 .asm)
$(BUILD_DIR)/%.o: %.asm
	@echo "[ASM]  $<"
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

# Compile ASM (AArch64 .S)
$(BUILD_DIR)/%.o: %.S
	@echo "[ASM]  $<"
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(KERNEL_BIN): $(KERNEL_OBJS) $(KERNEL_ASM_OBJS)
	@echo "[LD]   Enlazando kernel ($(ARCH))..."
	$(LD) $(LDFLAGS) -o $(KERNEL_ELF) $(KERNEL_OBJS) $(KERNEL_ASM_OBJS)
	@echo "[BIN]  Extrayendo binario..."
	$(OBJCOPY) -O binary $(KERNEL_ELF) $(KERNEL_BIN)

image: boot kernel
ifeq ($(ARCH), x86_64)
	@echo "[IMG]  Generando imagen de disco x86_64..."
	dd if=/dev/zero of=$(OS_IMAGE) bs=512 count=2880 2>/dev/null
	dd if=$(BOOT_BIN) of=$(OS_IMAGE) bs=512 conv=notrunc 2>/dev/null
	dd if=$(KERNEL_BIN) of=$(OS_IMAGE) bs=512 seek=9 conv=notrunc 2>/dev/null
else
	@echo "[IMG]  Generando imagen RPi4..."
	# Just copy kernel8.img
	cp $(KERNEL_BIN) $(OS_IMAGE)
endif

clean:
	rm -rf $(BUILD_DIR)

