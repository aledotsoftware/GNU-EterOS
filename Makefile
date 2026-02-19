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
AS   = nasm

# ---- Directorios ----
BOOT_DIR    = boot/$(ARCH)
KERNEL_DIR  = kernel
INCLUDE_DIR = include
BUILD_DIR   = build

# ---- Herramientas ----
QEMU    ?= qemu-system-x86_64
OBJCOPY ?= objcopy

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
          -Os                  \
          -Ikernel/apps/doomgeneric/include \
          -Ikernel/apps/doomgeneric/doomgeneric \
          -I$(INCLUDE_DIR)     \
          -I$(KERNEL_DIR)/net/lwip/src/include \
          -I$(KERNEL_DIR)/net/lwip_port

LDFLAGS = -T $(BOOT_DIR)/linker.ld \
          -nostdlib

# ---- Archivos fuente ----
BOOT_SRC    = $(BOOT_DIR)/boot.asm

# ---- lwIP Sources ----
LWIP_DIR = $(KERNEL_DIR)/net/lwip
LWIP_PORT_DIR = $(KERNEL_DIR)/net/lwip_port

LWIP_CORE_SRCS = $(LWIP_DIR)/src/core/init.c \
                 $(LWIP_DIR)/src/core/def.c \
                 $(LWIP_DIR)/src/core/dns.c \
                 $(LWIP_DIR)/src/core/inet_chksum.c \
                 $(LWIP_DIR)/src/core/ip.c \
                 $(LWIP_DIR)/src/core/mem.c \
                 $(LWIP_DIR)/src/core/memp.c \
                 $(LWIP_DIR)/src/core/netif.c \
                 $(LWIP_DIR)/src/core/pbuf.c \
                 $(LWIP_DIR)/src/core/raw.c \
                 $(LWIP_DIR)/src/core/stats.c \
                 $(LWIP_DIR)/src/core/sys.c \
                 $(LWIP_DIR)/src/core/tcp.c \
                 $(LWIP_DIR)/src/core/tcp_in.c \
                 $(LWIP_DIR)/src/core/tcp_out.c \
                 $(LWIP_DIR)/src/core/timeouts.c \
                 $(LWIP_DIR)/src/core/udp.c

LWIP_IPV4_SRCS = $(LWIP_DIR)/src/core/ipv4/acd.c \
                 $(LWIP_DIR)/src/core/ipv4/autoip.c \
                 $(LWIP_DIR)/src/core/ipv4/dhcp.c \
                 $(LWIP_DIR)/src/core/ipv4/etharp.c \
                 $(LWIP_DIR)/src/core/ipv4/icmp.c \
                 $(LWIP_DIR)/src/core/ipv4/igmp.c \
                 $(LWIP_DIR)/src/core/ipv4/ip4_frag.c \
                 $(LWIP_DIR)/src/core/ipv4/ip4.c \
                 $(LWIP_DIR)/src/core/ipv4/ip4_addr.c

LWIP_NETIF_SRCS = $(LWIP_DIR)/src/netif/ethernet.c

LWIP_PORT_SRCS = $(LWIP_PORT_DIR)/ethernetif.c \
                 $(LWIP_PORT_DIR)/sys_arch.c

LWIP_SRCS = $(LWIP_CORE_SRCS) $(LWIP_IPV4_SRCS) $(LWIP_NETIF_SRCS) $(LWIP_PORT_SRCS)

# ---- Doomgeneric Sources ----
DOOM_ROOT = kernel/apps/doomgeneric
DOOM_GENERIC_DIR = $(DOOM_ROOT)/doomgeneric

DOOM_ALL_SRCS = $(wildcard $(DOOM_GENERIC_DIR)/*.c)

DOOM_EXCLUDE = $(DOOM_GENERIC_DIR)/doomgeneric_win.c \
               $(DOOM_GENERIC_DIR)/doomgeneric_sdl.c \
               $(DOOM_GENERIC_DIR)/doomgeneric_xlib.c \
               $(DOOM_GENERIC_DIR)/doomgeneric_soso.c \
               $(DOOM_GENERIC_DIR)/doomgeneric_sosox.c \
               $(DOOM_GENERIC_DIR)/doomgeneric_linuxvt.c \
               $(DOOM_GENERIC_DIR)/doomgeneric_emscripten.c \
               $(DOOM_GENERIC_DIR)/doomgeneric_allegro.c \
               $(DOOM_GENERIC_DIR)/i_allegromusic.c \
               $(DOOM_GENERIC_DIR)/i_allegrosound.c \
               $(DOOM_GENERIC_DIR)/i_sdlmusic.c \
               $(DOOM_GENERIC_DIR)/i_sdlsound.c \
               $(DOOM_GENERIC_DIR)/dummy.c

DOOM_CORE_SRCS = $(filter-out $(DOOM_EXCLUDE), $(DOOM_ALL_SRCS))

DOOM_ETEROS_SRCS = $(DOOM_ROOT)/doom_libc.c \
                   $(DOOM_ROOT)/doomgeneric_eteros.c

DOOM_SRCS = $(DOOM_CORE_SRCS) $(DOOM_ETEROS_SRCS)

KERNEL_SRCS = $(KERNEL_DIR)/main.c              \
              $(KERNEL_DIR)/klog.c               \
              $(KERNEL_DIR)/string.c             \
              $(KERNEL_DIR)/stdio.c              \
              $(KERNEL_DIR)/futex.c              \
              $(KERNEL_DIR)/shell/shell.c        \
              $(KERNEL_DIR)/shell/commands.c     \
              $(KERNEL_DIR)/shell/history.c      \
              $(KERNEL_DIR)/shell/cmd_system.c   \
              $(KERNEL_DIR)/shell/cmd_net.c      \
              $(KERNEL_DIR)/shell/cmd_task.c     \
              $(KERNEL_DIR)/shell/cmd_misc.c     \
              $(KERNEL_DIR)/drivers/video/vga.c  \
              $(KERNEL_DIR)/drivers/serial/serial.c \
              $(KERNEL_DIR)/drivers/input/keyboard.c \
              $(KERNEL_DIR)/arch/x86_64/hal_impl.c \
              $(KERNEL_DIR)/arch/x86_64/idt.c    \
              $(KERNEL_DIR)/arch/x86_64/pic.c     \
              $(KERNEL_DIR)/drivers/timer/pit.c    \
              $(KERNEL_DIR)/drivers/rtc/rtc.c      \
              $(KERNEL_DIR)/arch/x86_64/boot/nvram.c \
              $(KERNEL_DIR)/mm/heap.c             \
              $(KERNEL_DIR)/apps/santitravel.c     \
              $(KERNEL_DIR)/apps/sysmon.c          \
              $(KERNEL_DIR)/apps/user_loader.c     \
              $(KERNEL_DIR)/drivers/pci/pci.c      \
              $(KERNEL_DIR)/fs/initrd.c            \
              $(KERNEL_DIR)/fs/vfs.c               \
              $(KERNEL_DIR)/fs/devfs.c             \
              $(KERNEL_DIR)/fs/procfs.c            \
              $(KERNEL_DIR)/task.c                 \
              $(KERNEL_DIR)/drivers/video/framebuffer.c \
              $(KERNEL_DIR)/drivers/video/font.c   \
              $(KERNEL_DIR)/mm/pmm.c               \
              $(KERNEL_DIR)/mm/vmm.c               \
              $(KERNEL_DIR)/drivers/input/mouse.c  \
              $(KERNEL_DIR)/arch/x86_64/gdt.c \
              $(KERNEL_DIR)/arch/x86_64/acpi.c \
              $(KERNEL_DIR)/arch/x86_64/smp.c \
              $(KERNEL_DIR)/arch/x86_64/apic.c \
              $(KERNEL_DIR)/arch/x86_64/syscall.c \
              $(KERNEL_DIR)/drivers/disk/partition.c \
              $(KERNEL_DIR)/fs/elf.c \
              $(KERNEL_DIR)/net/ip_utils.c \
              $(KERNEL_DIR)/net/stack.c \
              $(KERNEL_DIR)/net/tcp.c \
              $(KERNEL_DIR)/net/dhcp.c \
              $(KERNEL_DIR)/net/dhcp_parser.c \
              $(KERNEL_DIR)/net/raw_tcp.c \
              $(KERNEL_DIR)/drivers/net/e1000.c \
              $(KERNEL_DIR)/apps/wget.c

KERNEL_ASM_SRCS = $(KERNEL_DIR)/arch/x86_64/context_switch.asm \
                  $(KERNEL_DIR)/arch/x86_64/gdt_flush.asm \
                  $(KERNEL_DIR)/arch/x86_64/interrupts.asm \
                  $(KERNEL_DIR)/arch/x86_64/syscall_entry.asm \
                  $(KERNEL_DIR)/arch/x86_64/trampoline.asm \
                  $(KERNEL_DIR)/arch/x86_64/smp_trampoline_wrapper.asm \
                  $(KERNEL_DIR)/arch/x86_64/user_mode.asm \
                  $(KERNEL_DIR)/arch/x86_64/user_payload.asm

# ---- Archivos objeto ----
# Mapear .c -> .o en el directorio de build
KERNEL_OBJS = $(patsubst %.c,$(BUILD_DIR)/%.o,$(KERNEL_SRCS)) \
              $(patsubst %.asm,$(BUILD_DIR)/%.o,$(KERNEL_ASM_SRCS))

# ---- Archivos de salida ----
BOOT_BIN    = $(BUILD_DIR)/boot.bin
KERNEL_ELF  = $(BUILD_DIR)/kernel.elf
KERNEL_BIN  = $(BUILD_DIR)/kernel.bin
OS_IMAGE    = $(BUILD_DIR)/eteros.img
INITRD_IMG  = $(BUILD_DIR)/initrd.img
INITRD_DIR  = initrd_root

# ---- UEFI Configuration ----
UEFI_DIR    = boot/$(ARCH)/uefi
BOOT_EFI    = $(BUILD_DIR)/bootx64.efi
UEFI_IMG    = $(BUILD_DIR)/uefi.img

CFLAGS_UEFI = -ffreestanding -fno-stack-protector -fno-stack-check \
              -fshort-wchar -mno-red-zone -maccumulate-outgoing-args \
              -fPIC -Wall -I$(UEFI_DIR)

LDFLAGS_UEFI = -nostdlib -shared -Bsymbolic -T $(UEFI_DIR)/linker.ld

# =============================================================================
# Targets
# =============================================================================

.PHONY: all boot kernel image uefi run run-nographic debug clean dirs info userspace

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
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/shell
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/drivers/video
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/drivers/serial
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/drivers/input
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/arch/x86_64
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/arch/x86_64/boot
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/arch/xtensa
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/drivers/timer
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/drivers/rtc
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/mm
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/apps
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/drivers/net
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/net
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/net/lwip/src/core
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/net/lwip/src/core/ipv4
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/net/lwip/src/netif
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/net/lwip_port
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/drivers/pci
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/drivers/disk
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/mm
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/fs
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/crypto
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/apps/doomgeneric
	@mkdir -p $(BUILD_DIR)/$(KERNEL_DIR)/apps/doomgeneric/doomgeneric

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

# Compilar archivos .asm a .o (Kernel)
$(BUILD_DIR)/%.o: %.asm
	@echo "[ASM]  $<"
	$(AS) -f elf64 $< -o $@

# Enlazar objetos en un ELF, luego extraer binario plano
$(KERNEL_BIN): $(KERNEL_OBJS)
	@echo "[LD]   Enlazando kernel..."
	$(LD) $(LDFLAGS) -o $(KERNEL_ELF) $(KERNEL_OBJS)
	@echo "[BIN]  Extrayendo binario del kernel..."
	$(OBJCOPY) -O binary $(KERNEL_ELF) $(KERNEL_BIN)
	@echo "[INFO] Tamaño del kernel: $$(wc -c < $(KERNEL_BIN)) bytes"

# ---- Userspace ----
userspace:
	@echo "[USER] Compilando userspace..."
	$(MAKE) -C userspace
	@mkdir -p $(INITRD_DIR)
	cp userspace/test.elf $(INITRD_DIR)/

# ---- Initrd ----
initrd: userspace $(INITRD_IMG)

$(INITRD_IMG):
	@echo "[INITRD] Generando initrd..."
	@mkdir -p $(INITRD_DIR)
	./tools/mkinitrd.py $(INITRD_DIR) $(INITRD_IMG)

# ---- Imagen de disco ----
image: boot kernel initrd
ifeq ($(ARCH), x86_64)
	@echo "[IMG]  Generando imagen de disco..."
	@# Crear imagen vacía de 1.44 MB (formato floppy)
	dd if=/dev/zero of=$(OS_IMAGE) bs=512 count=2880 2>/dev/null
	@# Escribir bootloader completo (Stage 1 + Stage 2)
	dd if=$(BOOT_BIN) of=$(OS_IMAGE) bs=512 conv=notrunc 2>/dev/null
	@# Escribir kernel después del bootloader
	@# Stage 1 = 1 sector, Stage 2 = 16 sectores, kernel va en sector 17 (0-indexed seek)
	dd if=$(KERNEL_BIN) of=$(OS_IMAGE) bs=512 seek=17 conv=notrunc 2>/dev/null
	@# Escribir Initrd después del kernel (Sector 17 + 512 = 529)
	dd if=$(INITRD_IMG) of=$(OS_IMAGE) bs=512 seek=529 conv=notrunc 2>/dev/null
	@echo "[IMG]  Imagen generada: $(OS_IMAGE) ($$(wc -c < $(OS_IMAGE)) bytes)"
else
	@echo "[IMG]  Generación de imagen para $(ARCH) no implementada."
endif

# ---- UEFI Bootloader ----
uefi: dirs kernel initrd $(UEFI_IMG)

$(BOOT_EFI): $(UEFI_DIR)/uefi_boot.c
	@echo "[CC]   Compiling UEFI Bootloader..."
	$(CC) $(CFLAGS_UEFI) -c $< -o $(BUILD_DIR)/uefi_boot.o
	@echo "[LD]   Linking UEFI Bootloader..."
	$(LD) $(LDFLAGS_UEFI) $(BUILD_DIR)/uefi_boot.o -o $(BUILD_DIR)/bootx64.so
	@echo "[OBJCOPY] Creating PE32+ Image..."
	$(OBJCOPY) -j .text -j .sdata -j .data -j .dynamic -j .dynsym  -j .rel \
	           -j .rela -j .rel.* -j .rela.* -j .rel* -j .rela* \
	           -j .reloc -O pei-x86-64 --subsystem=10 \
	           $(BUILD_DIR)/bootx64.so $@

$(UEFI_IMG): $(BOOT_EFI) $(KERNEL_BIN) $(INITRD_IMG)
	@echo "[IMG]  Generating UEFI FAT32 Image..."
	./tools/mkuefi.py $@ $(BOOT_EFI):EFI/BOOT/BOOTX64.EFI $(KERNEL_BIN):kernel.bin $(INITRD_IMG):initrd.img

# ---- Ejecutar en QEMU ----
run: all
ifeq ($(ARCH), x86_64)
	@echo "[QEMU] Iniciando eterOS..."
	$(QEMU) -drive format=raw,file=$(OS_IMAGE),index=0,if=ide,media=disk \
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
	$(QEMU) -drive format=raw,file=$(OS_IMAGE),index=0,if=ide,media=disk \
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
