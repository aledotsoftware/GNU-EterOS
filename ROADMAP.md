# 🚀 Hoja de Ruta (Roadmap hacia la GUI)

Este documento detalla el estado actual del desarrollo de éterOS, verificado contra el código fuente (`kernel/`, `userspace/`, `include/`).

## Estado General
- **Arquitectura:** x86_64 (Long Mode)
- **Gestión de Memoria:** Paging de 4 niveles, Huge Pages, Heap Dinámico.
- **Gráficos:** Motor Omni v2.0 (Software Rendering optimizado).
- **Multitarea:** Scheduler Round-Robin (Preemptivo).
- **Red:** Stack lwIP 2.2.0 (Raw API) + Driver Intel e1000.

---

### Fase 1: Estabilización del Núcleo (Base) ✅
- [x] **GDT & TSS:** Segmentación de 64 bits y Task State Segment para manejo de stacks de interrupción (`kernel/arch/x86_64/gdt.c`).
- [x] **IDT & ISRs:** Gestión de excepciones (Page Fault, GPF) y remapeo del PIC 8259A (256 vectores) (`kernel/arch/x86_64/idt.c`).
- [x] **PMM (Gestor de Memoria Física):** Mapa de bits para páginas de 4 KB, soporte E820, detección automática de RAM (`kernel/mm/pmm.c`).
- [x] **VMM (Gestor de Memoria Virtual):** Mapeo dinámico de páginas (4-Level Paging), detección de Huge Pages (`kernel/mm/vmm.c`).
- [x] **Heap Dinámico:** `kmalloc()`, `kfree()`, `kcalloc()` con first-fit y coalescing (8 MB iniciales) (`kernel/memory/heap.c`).
- [x] **Estabilidad:** Bootloader corregido (Stage 2 expandido), PMM underflow fix, layout de imagen alineado.

### Fase 2: Drivers y E/S Esencial ✅
- [x] **Teclado PS/2 Mejorado:** Scancodes Set 1 + Extended (0xE0), flechas, Home/End/Delete/PgUp/PgDown (`kernel/drivers/input/keyboard.c`).
- [x] **Optimizaciones de Rendimiento:** VGA Scroll (64-bit mov), Serial DMA-like Burst, Shell no-bloqueante.
- [x] **Shell con Historial:** Flecha ⬆️/⬇️ para navegar comandos anteriores (8 entradas) (`kernel/shell.c`).
- [x] **Temporizador PIT:** 100 Hz, API de `uptime`, `timer_wait()` para delays (`kernel/drivers/timer.c`).
- [x] **Comando `uptime`:** Muestra horas/minutos/segundos desde boot.
- [x] **Comando `sysinfo` mejorado:** RAM real (PMM), timer, uptime dinámico.
- [x] **Driver NIC (e1000):** Intel PRO/1000 NIC driver con dirección MAC y escaneo PCI (`kernel/drivers/net/e1000.c`).
- [x] **Cliente DHCP:** Discover/Offer básico para obtener IP (`kernel/net/dhcp.c`).
- [x] **Driver de Video VBE/GOP:** Framebuffer de alta resolución (Linear Framebuffer) (`kernel/drivers/video/framebuffer.c`).
- [x] **Terminal Gráfica:** Logger en pantalla usando Framebuffer (sysmon, panics).
- [x] **Mouse PS/2:** Soporte básico para puntero en pantalla (`kernel/drivers/input/mouse.c`).
- [x] **Bus PCI:** Enumeración de dispositivos y lectura de configuración (`kernel/drivers/pci.c`).
- [x] **RTC (Real Time Clock):** Lectura de tiempo CMOS (`kernel/drivers/rtc.c`).

### Fase 2.5: Compatibilidad de Arranque (Boot Compatibility) ✅
- [x] **PXE Boot (Network):** Soporte para arranque sin disco via TFTP (SiS191/Universal PXE).
- [x] **USB Legacy Boot:** Soporte para BIOS antiguos con emulación HDD (Fake BPB, MBR Patch).
- [x] **Initrd (Initial Ramdisk):** Sistema de archivos en memoria para assets (Logos, Configs) (`kernel/fs/initrd.c`).

---

### Fase 5.2: Topología Dinámica y SMP (Multicore) 🚧
*Estado: Infraestructura lista, Lógica de ejecución pendiente.*

- [x] **Detección ACPI Avanzada:** Stack ACPI completo para parsear tablas RSDP, RSDT y MADT (`kernel/arch/x86_64/acpi.c`).
- [x] **Topología Dinámica (>64 Cores):** Implementación de `cpu_set_t` escalable (bitmaps dinámicos) y `MAX_CPUS` (256) en `include/cpu.h`.
- [x] **Per-CPU Data (GS Base):** Uso del registro `GS` (MSR `GS_BASE` / `KERNEL_GS_BASE`) para almacenamiento local de hilo (TLS) y contexto de CPU (`kernel/arch/x86_64/smp.c`).
- [x] **BSP Initialization:** Bootstrapping del núcleo principal con soporte para stack de syscalls independiente.
- [~] **APIC Driver:** Detección de LAPIC/IOAPIC vía MADT implementada, pero el sistema aún usa el PIC 8259A legado para interrupciones (`kernel/arch/x86_64/pic.c`).
- [ ] **SMP Trampoline:** Código de 16-bit para despertar Application Processors (APs) (Pendiente).
- [ ] **Scheduler Multicore:** Balanceo de carga y colas de ejecución por CPU (El scheduler actual es Round-Robin Single-Core en `kernel/task.c`).

**Estrategia Técnica para SMP (>64 Cores):**
1.  **Abstracción de Topología:** Uso de `cpu_set_t` (bitmap de 256 bits) en lugar de `uint64_t`.
2.  **Inicialización Simétrica:** BSP inicializa estructuras globales, APs despiertan via INIT-SIPI-SIPI.
3.  **Planificación:** Se requiere implementar colas `per-cpu` y Work Stealing.

---

### Fase 3: Kernel Moderno y Multitarea ✅
- [x] **Heap Manager:** `kmalloc()`, `kfree()`, `kcalloc()` implementados.
- [x] **Scheduler Round-Robin:** Multitarea preemptiva (comandos `ps`, `kill`, `demo`) - `kernel/task.c`.
- [x] **VFS (Virtual File System):** Abstracción de sistemas de archivos (`kernel/fs/vfs.c`).
- [x] **Initrd:** Carga de módulos y assets.

### Fase 3.5: Networking & Storage ✅
- [x] **Driver NIC:** Intel PRO/1000 (e1000).
- [x] **Cliente DHCP:** Funcional.
- [x] **Integración lwIP:** Stack TCP/IP completo (lwIP 2.2.0 + Porting layer en `kernel/net/lwip_port`).
- [x] **Soporte FAT32:** Lectura de archivos y directorios (8.3).
- [x] **Wget:** Downloader HTTP básico (`kernel/apps/wget.c`).

### Fase 3.6: Infraestructura de Actualización y Red ✅
- [x] **Stack de Red Minimalista (TCP/IP):** Sockets básicos (`kernel/net/tcp.c`).
- [x] **Módulo de Criptografía:** SHA-256 y Ed25519 (verificación de firmas) (`kernel/crypto/`).
- [x] **Driver de Almacenamiento A/B:** Particionado MBR y slots (`kernel/drivers/disk/partition.c`).
- [x] **Sistema de Commits Atómicos:** Estructura en NVRAM (`kernel/arch/x86_64/boot/nvram.c`).
- [x] **Build Update Tool:** Script de empaquetado `.zst` (`tools/updater/build_update.ps1`).

---

### Fase 4: Espacio de Usuario (Userland) 🚧
- [x] **Context Switching:** Transición segura Ring 0 → Ring 3 (`iretq`, TSS RSP0).
- [x] **Syscalls (SYSCALL/SYSRET):** Mecanismo MSR (`EFER`, `STAR`, `LSTAR`) habilitado (`kernel/arch/x86_64/syscall.c`).
- [x] **User Loader:** Carga de payload binario en 0x40000000 (`kernel/apps/user_loader.c`).
- [x] **Memoria de Usuario:** Separación de espacio kernel/usuario (PML4 User bit).
- [x] **Cargador ELF64:** Relocación y carga de segmentos.
- [~] **Multiprocesamiento (fork/clone):** Soporte de Kernel Threads (`task_create`), pero falta implementación de `fork()` en userspace para duplicación de espacios de dirección.

### Fase 4.5: Compatibilidad POSIX 🚧
- [~] **Tabla de Syscalls Linux:** Implementados: `open`, `close`, `read`, `write`, `lseek`, `kill`, `exit`, `getpid`, `sched_yield`, `brk`, `arch_prctl`, `uname`, `ioctl` (stub), `readv`, `writev`.
- [~] **Portar musl libc:** Librería C minimalista iniciada (`userspace/libc/src/stdio.c`, `start.c`), pero incompleta.
- [~] **Soporte de señales:** `sys_kill` implementado para terminación (SIGKILL), manejo de señales personalizado pendiente.
- [x] **Estructura `/dev`, `/proc`:** Implementados `/dev/null`, `/dev/zero`, `/dev/tty`, `/proc/uptime`, `/proc/version`.

---

### Fase 5: Entorno Gráfico (Flux UI & AetherGraphics) ✅
- [x] **Motor de Dibujo "Omni":** Primitivas 2D y decodificador PNG Nativo (`kernel/ui/upng.c`).
- [x] **Double Buffering Activo:** Renderizado en RAM antes de flush (`kernel/ui/image.c`).
- [x] **Event Loop Reactivo:** Sistema de despacho de mensajes mouse/teclado.
- [x] **Compositor de Ventanas:** Gestión de apilamiento y transparencia (`kernel/ui/window.c`).
- [x] **Flux UI Experience:** Apps integradas (`gui_demo.c`):
    - Terminal Flux, Monitor de Sistema, Admin. de Dispositivos.
    - SantiTravel, The Matrix.
    - Navegador Eter (con lwIP), Utilidades.

### Fase 5.1: Optimización del Motor Gráfico (Omni v2.0) ✅
*Verificado en `kernel/ui/omni.c`*

- [x] **Motor Omni v2.0:** Refactorización completa:
    - ⚡ **Frame Batching:** `omni_begin_frame()` cachea punteros (FB pointer caching).
    - ⚡ **Dirty Region Tracking:** Rastreo de regiones sucias (`dirty_mark`, `omni_get_dirty_rect`) para refresco parcial.
    - ⚡ **Alpha Blending 256-nivel:** Composición real (`_alpha_blend`) implementada.
    - ⚡ **Líneas Optimizadas:** Bresenham con escritura directa a memoria.
    - ⚡ **Bitmap Blitting por Filas:** `memcpy` para filas opacas.
    - ⚡ **Texto Transparente:** `omni_draw_char_transparent()` implementado.
- [x] **Nuevas Primitivas 2D:**
    - 🎨 **Gradientes Verticales:** `omni_fill_gradient_v`.
    - 🎨 **Rectángulos Redondeados:** `omni_draw_rounded_rect`.
    - 🎨 **Círculos Rellenos:** `omni_fill_circle` (Midpoint algo).
- [x] **Optimización de Aplicaciones:** Notificaciones con Fade-In/Out y tarjetas con gradiente implementadas en la UI.
- [ ] **Aceleración por GPU:** VirtIO-GPU pendiente.
