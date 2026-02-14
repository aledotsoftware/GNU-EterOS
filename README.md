# 🌌 éterOS: El Sistema Operativo Universal

éterOS es un sistema operativo desarrollado desde el **"cero absoluto" (Bare-Metal)** diseñado para correr en **cualquier dispositivo**: desde un microcontrolador en un aire acondicionado hasta un cluster de servidores en un data center. Un solo kernel, una sola filosofía, infinitas plataformas.

> *"El éter lo llena todo, invisible e infinito. éterOS: Un sistema operativo que no conoce límites."*

## ✨ Filosofía y Visión

El nombre **éter** evoca la sustancia que lo llena todo de forma invisible. Bajo esa premisa, éterOS se construye sobre tres pilares:

- **Ether-Core (Arquitectura Híbrida):** La estabilidad de un microkernel con el rendimiento de un núcleo monolítico. Drivers críticos corren en espacio de usuario para evitar Kernel Panics.
- **Fluidez Nativa:** Optimizado para hardware moderno (UEFI, NVMe, Multi-core), ignorando el lastre del hardware de los 90.
- **Aero-Minimalismo:** Una interfaz visual basada en vectores y renderizado por GPU (Framebuffer moderno) para una experiencia visual de alta fidelidad.

### 🎯 Objetivos Críticos

| Objetivo | Meta |
|---|---|
| **Tamaño total** | < 10 MB (Core). *Nota: Capas de compatibilidad y Assets gráficos aumentarán este tamaño.* |
| **Arquitecturas** | x86_64, aarch64, ARM Cortex-M, RISC-V, Xtensa, AVR |
| **Portabilidad** | HAL universal con tiers Micro / Core / Full |
| **Compatibilidad** | Capa POSIX para ejecutar software como Apache |
| **Estética** | Tema "Austral Aurora" — modos oscuros con acentos de luz |

### 💻 Dónde corre éterOS

| Tier | Edición | Dispositivos | Arquitecturas | Memoria / Protección |
|---|---|---|---|---|
| **Tier 1** | éterOS-Micro | Aire acondicionado, sensores IoT, drones | ARM Cortex-M, AVR, RISC-V32, Xtensa | **No MMU** / MPU (Static) |
| **Tier 2** | éterOS-Core | Tablets, phones, RPi, satélites | aarch64, RISC-V64 | **MMU** / Paging (4-Level) |
| **Tier 3** | éterOS-Full | PCs, servidores, data centers | x86_64 | **MMU** / Ring 0-3 Protection |

## 📂 Estructura del Proyecto

```
éterOS/
├── boot/                       # El despertar del metal (Bootloader)
│   └── x86_64/
│       ├── boot.asm            # MBR Legacy (BIOS) - Stage 1 + Stage 2
│       └── linker.ld           # Script de enlace para el kernel
├── kernel/                     # Ether-Core: El corazón del sistema
│   ├── main.c                  # Punto de entrada (kmain)
│   ├── string.c                # Utilidades de cadena y memoria
│   ├── shell.c                 # Terminal interactiva (historial, teclas extendidas)
│   ├── arch/                   # HAL — Una carpeta por arquitectura
│   │   ├── x86_64/             # ✅ Implementado (PC / servidores)
│   │   │   ├── idt.c           # IDT + ISRs (256 vectores)
│   │   │   ├── gdt.c           # GDT + TSS (64-bit)
│   │   │   ├── pic.c           # PIC 8259 remapeado
│   │   │   └── interrupts.asm  # Stubs de interrupción ASM
│   │   ├── aarch64/            # 🔜 Planificado (RPi, phones, satélites)
│   │   ├── arm-cortex-m/       # 🔜 Planificado (STM32, Pico, IoT)
│   │   ├── riscv64/            # 🔜 Planificado (SiFive, StarFive)
│   │   └── xtensa/             # 🔜 Planificado (ESP32)
│   ├── mm/                     # Gestión de Memoria
│   │   ├── pmm.c               # Physical Memory Manager (bitmap, E820)
│   │   ├── vmm.c               # Virtual Memory Manager (4-Level Paging)
│   │   └── heap.c              # Heap dinámico (kmalloc/kfree/kcalloc)
│   ├── task.c                  # Scheduler Round-Robin (Multitarea)
│   ├── drivers/                # El sistema sensorial
│   │   ├── video/              # Drivers de Video
│   │   │   ├── vga.c           # Modo Texto Legacy
│   │   │   ├── framebuffer.c   # Linear Framebuffer (GOP/VBE)
│   │   │   └── font.c          # Fuente Bitmap 8x16
│   │   ├── serial/serial.c     # UART 16550 para depuración
│   │   ├── input/keyboard.c    # Teclado PS/2 (Set 1 + Extended, IRQ1)
│   │   ├── timer/pit.c         # PIT 8254 @ 100 Hz (uptime, delays)
│   │   └── net/e1000.c         # Intel PRO/1000 NIC driver
│   ├── ui/                     # AetherGraphics (GUI System)
│   │   ├── window.c            # Window Manager & Compositor
│   │   └── primitives.c        # Primitivas de dibujo 2D
│   ├── net/                    # Stack de Red
│   │   └── dhcp.c              # Cliente DHCP (Discover/Offer)
│   └── apps/                   # Aplicaciones del kernel
│       ├── santitravel.c       # Juego de texto (aventuras)
│       ├── sysmon.c            # Monitor del sistema
│       └── gui_demo.c          # Flux UI Demo (Multitarea gráfica)
├── include/                    # API del sistema
│   ├── hal.h                   # 🌍 HAL universal (interfaz multi-arch)
│   ├── types.h                 # Tipos freestanding
│   ├── idt.h / gdt.h / pic.h   # Interrupciones y segmentación (x86_64)
│   ├── pmm.h / vmm.h / mm.h    # Memoria (física, virtual, heap)
│   ├── keyboard.h / shell.h    # Input y terminal
│   ├── timer.h                 # API del PIT timer
│   ├── pci.h                   # Escaneo de bus PCI
│   ├── vga.h / serial.h        # Video y debug
│   └── io.h / string.h         # I/O y utilidades
├── scripts/                    # Herramientas de despliegue
│   ├── setup_wsl.sh            # Setup para WSL2
│   ├── setup_windows.ps1       # Setup para Windows nativo
│   ├── setup_toolchain.sh      # Setup para Linux/macOS
│   └── create_iso.sh           # Generar ISO booteable
├── .github/workflows/
│   └── build.yml               # CI/CD: Compilación y Release automático
├── build/                      # Artefactos generados
├── Makefile                    # Build system (Linux/WSL)
├── build.ps1                   # Build system (Windows nativo)
├── JULES.md                    # Instrucciones para Jules Agent
└── README.md                   # Este archivo
```

## 🛠️ Módulos de Nueva Era

### 1. El Salto al Modo Largo (x86_64)

El arranque de éterOS configura directamente un entorno de **64 bits puro**:

- **Legacy Boot (BIOS):** MBR (Stage 1) carga Stage 2 (16 sectores) + Kernel (128 sectores). Stage 2 detecta memoria E820, configura GDT y Paginación.
- **Modern Boot (UEFI):** *[En desarrollo]* Carga directa de binario PE/COFF.
- **Identity Mapping:** 4 GB con huge pages (2 MB cada una) para acceso directo al hardware. El kernel y heap residen dentro de esta región.
- **E820 Memory Detection:** El bootloader detecta la memoria disponible via BIOS INT 0x15, E820 y la pasa al kernel en 0x5000.

### 2. AetherGraphics (Video)

- **VGA Legacy:** Buffer de texto en 0xB8000 (80×25, 16 colores) — implementado
- **Framebuffer Moderno:** Soporte planeado para UEFI/GOP con Linear Frame Buffer
- **Double Buffering:** Renderizado en RAM antes de copiar al Framebuffer (sin parpadeos)

### 3. HAL (Hardware Abstraction Layer)

Diseñado para portabilidad. Toda la lógica del kernel es agnóstica al hardware; Ether-Core habla con la HAL, facilitando expansión futura a RISC-V o ARM.

### 4. Serial Debug (COM1)

UART 16550 a 38400 baud para depuración. Compatible con `qemu -serial stdio`.

## 📋 Layout de Memoria

```
Dirección       | Contenido
----------------|----------------------------------
0x00000-0x003FF | IVT (Interrupt Vector Table)
0x00400-0x004FF | BIOS Data Area (BDA)
0x05000-0x05FFF | Mapa de Memoria E820 (del bootloader)
0x07C00-0x07DFF | Stage 1 - MBR (512 bytes)
0x07E00-0x0BDFF | Stage 2 - Bootloader extendido (16 sectores)
0x10000-0x1FFFF | Ether-Core (Kernel de éterOS, ~38 KB)
0x1A000-0x1BFFF | PMM Bitmap (~4 KB para 128 MB)
0x70000-0x76FFF | Tablas de paginación bootloader (28 KB)
0x90000         | Tope del Stack
0xB8000-0xBFFFF | Buffer de video VGA
0x00400000      | Heap del kernel (8 MB, identity-mapped)
```

## 🚀 Hoja de Ruta (Roadmap hacia la GUI)

### Fase 1: Estabilización del Núcleo (Base) ✅
- [x] **GDT & TSS:** Segmentación de 64 bits y Task State Segment para manejo de stacks de interrupción
- [x] **IDT & ISRs:** Gestión de excepciones (Page Fault, Double Fault, GPF) y remapeo del PIC (256 vectores)
- [x] **PMM (Gestor de Memoria Física):** Mapa de bits para páginas de 4 KB, soporte E820, detección automática de RAM
- [x] **VMM (Gestor de Memoria Virtual):** Mapeo dinámico de páginas (4-Level Paging), detección de Huge Pages
- [x] **Heap Dinámico:** `kmalloc()`, `kfree()`, `kcalloc()` con first-fit y coalescing (8 MB)
- [x] **Estabilidad:** Bootloader corregido (Stage 2 expandido a 8 KB), PMM underflow fix, layout de imagen alineado

### Fase 2: Drivers y E/S Esencial (En progreso)
- [x] **Teclado PS/2 Mejorado:** Scancodes Set 1 + Extended (0xE0), flechas, Home/End/Delete/PgUp/PgDown
- [x] **Optimizaciones de Rendimiento:** VGA Scroll (64-bit mov), Serial DMA-like Burst, Shell no-bloqueante
- [x] **Shell con Historial:** Flecha ⬆️/⬇️ para navegar comandos anteriores (8 entradas)
- [x] **Temporizador PIT:** 100 Hz, API de `uptime`, `timer_wait()` para delays
- [x] **Comando `uptime`:** Muestra horas/minutos/segundos desde boot
- [x] **Comando `sysinfo` mejorado:** RAM real (PMM), timer, uptime dinámico
- [x] **Driver NIC (e1000):** Intel PRO/1000 con dirección MAC y escaneo PCI
- [x] **Cliente DHCP:** Discover/Offer básico para obtener IP
- [x] **Driver de Video VBE/GOP:** Framebuffer de alta resolución (Linear Framebuffer)
- [ ] **Terminal Gráfica:** Logger en pantalla usando Framebuffer
- [ ] **Mouse PS/2:** Soporte para puntero en pantalla

### Fase 3: Kernel Moderno y Multitarea
- [x] **Heap Manager:** `kmalloc()`, `kfree()`, `kcalloc()` para el kernel (completado en Fase 1)
- [x] **Scheduler Round-Robin:** Multitarea preemptiva (comandos `ps`, `kill`, `demo`) - `kernel/task.c`
- [ ] **VFS (Virtual File System):** Abstracción de sistemas de archivos
- [ ] **Initrd:** Sistema de archivos en RAM (sin drivers de disco complejos)

### Fase 3.5: Networking & Storage
- [x] **Driver NIC:** Intel PRO/1000 (e1000) con detección PCI y MAC
- [x] **Cliente DHCP:** Discover/Offer para obtención de IP
- [ ] **Integración lwIP:** Stack TCP/IP ligero para networking completo
- [ ] **Soporte FAT32:** Lectura básica de archivos

sistema de actualizaciones?

### Fase 4: Espacio de Usuario (Userland)
- [ ] **Context Switching:** Transición Ring 0 → Ring 3
- [ ] **Syscalls (SYSCALL/SYSRET):** Instrucciones nativas x86_64 (no `int 0x80`)
- [ ] **Memoria de Usuario:** Separación de espacio kernel/usuario
- [ ] **Cargador ELF64:** Cargar y ejecutar binarios externos
- [ ] **Multiprocesamiento (fork/clone):** Duplicar procesos para paralelismo

### Fase 4.5: Compatibilidad POSIX
- [ ] **Tabla de Syscalls Linux:** Las 50 syscalls esenciales (read, write, open, close, fork, execve)
- [ ] **Portar musl libc:** Librería C minimalista para aplicaciones
- [ ] **Soporte de señales:** SIGKILL, SIGSEGV, etc.
- [ ] **Estructura `/dev`, `/proc`:** Nodos de dispositivos virtuales

### Fase 5: Entorno Gráfico (AetherGraphics)
- [x] **Motor de Dibujo 2D:** Primitivas: píxeles, líneas, rectángulos, fuentes bitmap (`kernel/ui/primitives.c`)
- [x] **Double Buffering:** Renderizado sin parpadeo (Backbuffer en RAM)
- [ ] **Event Loop:** Sistema de mensajes (mouse, teclado → ventanas)
- [x] **Gestor de Ventanas (Compositor):** Superposición de ventanas con foco (Flux UI - `kernel/ui/window.c`)
- [x] **Primera App:** Flux UI Demo (gui_demo.c) con multitarea visual

### Fase 5.5: Subsistema de Compatibilidad Linux (Aether-Linux-Subsystem)
*Objetivo: Ejecutar binarios ELF de Linux sin máquinas virtuales.*
- [ ] **Traducción de Syscalls:** Mapeo de syscalls de Linux (ABI) a nativas de éterOS.
- [ ] **VFS Layer:** Implementación de `/proc`, `/sys`.
- [ ] **Linux Driver Wrapper:** Capa de pegamento (Glue Logic) para reutilizar drivers de red/wifi.

### Fase 5.6: Subsistema de Compatibilidad Android (Aether-Droid)
- [ ] **Binder IPC:** Implementación del mecanismo de comunicación entre procesos de Android.
- [ ] **Dalvik/ART Shim:** Capa para ejecutar el runtime de Android sobre éterOS.
- [ ] **HAL Wrapper:** Adaptador para drivers de hardware específicos de Android (libhardware).

### Fase 5.7: Subsistema de Compatibilidad Windows (Aether-Win32)
*Similar a WINE, pero a nivel de kernel.*
- [ ] **PE Loader:** Cargador de ejecutables `.exe` y librerías `.dll`.
- [ ] **Win32 API Bridge:** Reimplementación de `kernel32.dll`, `user32.dll` en userland.
- [ ] **NT Syscall Translation:** Traducción de llamadas nativas NT a éterOS.

### Fase 5.8: Subsistema de Compatibilidad macOS/iOS (Aether-Darwin)
*Inspirado en el proyecto Darling.*
- [ ] **Mach-O Loader:** Cargador de binarios de macOS.
- [ ] **Objective-C Runtime:** Soporte para librerías base de Apple.
- [ ] **Cocoa Bridge:** Mapeo de primitivas gráficas de Quartz a AetherGraphics.

### Fase 5.10: Subsistema de Compatibilidad Web (Aether-Web)
- [ ] **Chromium Embedded:** Port nativo del motor Blink.
- [ ] **PWA Runtime:** Ejecución de aplicaciones web como nativas (.crx, .wbn).




### Meta Final: Software Real
- [ ] **Apache HTTP Server:** Compilado estáticamente contra musl + lwIP, corriendo sobre éterOS

## ⚙️ Toolchain

| Herramienta | Uso |
|---|---|
| `nasm` | Ensamblador (bootloader) |
| `x86_64-elf-gcc` | Cross-compiler C (freestanding) |
| `x86_64-elf-ld` | Enlazador |
| `x86_64-elf-objcopy` | Conversión ELF → binario plano |
| `qemu-system-x86_64` | Emulador para pruebas |
| `gdb` | Depuración remota |

## 🔧 Compilación y Ejecución

### Opción 1: Jules Agent (Recomendado)
Jules compila en su VM Linux y pushea los binarios a GitHub.
Solo necesitás descargar `eteros.img` de la sección **Releases** y ejecutar:

```bash
qemu-system-x86_64 -drive format=raw,file=eteros.img,if=floppy -serial stdio -m 128M
```

### Opción 2: WSL2 en Windows
```bash
cd /mnt/p/EterOS
./scripts/setup_wsl.sh   # Una sola vez
make run
```

### Opción 3: Linux/macOS nativo
```bash
./scripts/setup_toolchain.sh  # Una sola vez
make run
```

### Comandos del Makefile
```bash
make all            # Compilar todo
make run            # Compilar y ejecutar en QEMU
make run-nographic  # Ejecutar sin ventana (ideal para WSL)
make debug          # Ejecutar con GDB server
make clean          # Limpiar artefactos
make info           # Ver información del toolchain
```

## 🛣️ Secuencia de Arranque

```
┌──────────────────────────────────────────────────────────┐
│ 1. BIOS carga el MBR (sector 1) en 0x7C00               │
│    └─▶ Stage 1 ejecuta                                   │
│                                                          │
│ 2. Stage 1: Habilita A20, carga Stage 2 (16 sect)       │
│    + Kernel (128 sect) desde disco                       │
│    └─▶ Salta a Stage 2 (0x7E00)                          │
│                                                          │
│ 3. Stage 2: Verifica Long Mode                           │
│    └─▶ Detecta memoria via E820 BIOS                     │
│                                                          │
│ 4. Stage 2: Configura GDT32                              │
│    └─▶ Entra en Modo Protegido (32-bit)                  │
│                                                          │
│ 5. Stage 2: Configura paginación (PML4/PDPT/PD, 4GB)    │
│    └─▶ Activa PAE + EFER.LME + CR0.PG                   │
│                                                          │
│ 6. Stage 2: Carga GDT64, salta a Long Mode               │
│    └─▶ Entra en Long Mode (64-bit)                       │
│                                                          │
│ 7. Long Mode: Salta a kmain() en 0x10000                 │
│    └─▶ GDT/TSS → IDT → PIC → Timer → PMM → VMM → Heap  │
│    └─▶ Shell interactivo con historial y teclas extendidas│
└──────────────────────────────────────────────────────────┘
```

## 🤖 Desarrollo con Jules

Este proyecto utiliza **Jules** (Google) como agente de compilación:

1. Jules recibe tareas de desarrollo (nuevas features, bug fixes)
2. Compila en su VM Linux con el cross-compiler
3. Verifica el arranque en QEMU
4. Pushea código + binarios (`build/eteros.img`) a GitHub
5. GitHub Actions crea Releases descargables

Ver [`JULES.md`](JULES.md) para instrucciones detalladas del agente.

## 📜 Licencia

© 2026 **Tudex Networks**. Todos los derechos reservados.
