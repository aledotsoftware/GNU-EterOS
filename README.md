# 🌌 éterOS: El Sistema Operativo de la Nueva Era

éterOS es un sistema operativo desarrollado desde el **"cero absoluto" (Bare-Metal)** para arquitectura **x86_64**. No es solo un kernel; es una propuesta de computación fluida, ligera y minimalista, diseñada para eliminar las capas de abstracción innecesarias del software moderno.

> *"El código que no existe no puede fallar. éterOS: Simplicidad absoluta sobre el metal."*

## ✨ Filosofía y Visión

El nombre **éter** evoca la sustancia que lo llena todo de forma invisible. Bajo esa premisa, éterOS se construye sobre tres pilares:

- **Ether-Core (Arquitectura Híbrida):** La estabilidad de un microkernel con el rendimiento de un núcleo monolítico. Drivers críticos corren en espacio de usuario para evitar Kernel Panics.
- **Fluidez Nativa:** Optimizado para hardware moderno (UEFI, NVMe, Multi-core), ignorando el lastre del hardware de los 90.
- **Aero-Minimalismo:** Una interfaz visual basada en vectores y renderizado por GPU (Framebuffer moderno) para una experiencia visual de alta fidelidad.

### 🎯 Objetivos Críticos

| Objetivo | Meta |
|---|---|
| **Tamaño total** | < 10 MB (kernel + drivers + apps + assets) |
| **Arquitectura** | x86_64 nativo (Long Mode) |
| **Portabilidad** | HAL preparada para RISC-V y ARM |
| **Compatibilidad** | Capa POSIX para ejecutar software como Apache |
| **Estética** | Tema "Austral Aurora" — modos oscuros con acentos de luz |

## 📂 Estructura del Proyecto

```
éterOS/
├── boot/                       # El despertar del metal (Bootloader)
│   └── x86_64/
│       ├── boot.asm            # Bootloader de dos etapas (Stage 1 + Stage 2)
│       └── linker.ld           # Script de enlace para el kernel
├── kernel/                     # Ether-Core: El corazón del sistema
│   ├── main.c                  # Punto de entrada (kmain)
│   ├── string.c                # Utilidades de cadena y memoria
│   └── drivers/                # El sistema sensorial
│       ├── video/vga.c         # AetherGraphics (VGA → GOP en futuro)
│       └── serial/serial.c     # UART 16550 para depuración
├── include/                    # API del sistema
│   ├── types.h                 # Tipos freestanding
│   ├── vga.h                   # API de video
│   ├── string.h                # API de cadenas
│   ├── serial.h                # API serial
│   └── io.h                    # E/S de puertos (inline)
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

- **Stage 1 (MBR):** Habilita A20, carga Stage 2 + Kernel desde disco
- **Stage 2:** Configura GDT, paginación de 4 niveles (PML4 → PDPT → PD), activa EFER.LME
- **Identity Mapping:** 8 MB de huge pages (2 MB cada una) para acceso directo al hardware

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
0x07C00-0x07DFF | Stage 1 - MBR (512 bytes)
0x07E00-0x09DFF | Stage 2 - Bootloader extendido
0x10000-0x1FFFF | Ether-Core (Kernel de éterOS)
0x70000-0x72FFF | Tablas de paginación (12 KB)
0x90000         | Tope del Stack
0xB8000-0xBFFFF | Buffer de video VGA
```

## 🚀 Hoja de Ruta (Roadmap hacia la GUI)

### Fase 1: Estabilización del Núcleo (Base)
- [ ] **GDT & TSS:** Segmentación de 64 bits y Task State Segment para manejo de stacks de interrupción
- [ ] **IDT & ISRs:** Gestión de excepciones (Page Fault, Double Fault, GPF)
- [ ] **PMM (Gestor de Memoria Física):** Mapa de bits para páginas de 4 KB, soporte para Huge Pages (2 MB)
- [ ] **VMM (Gestor de Memoria Virtual):** Mapeo dinámico de páginas y protección de memoria

### Fase 2: Drivers y E/S Esencial
- [ ] **Driver de Video GOP:** Framebuffer de alta resolución vía UEFI
- [ ] **Terminal de Emergencia:** Logger en pantalla usando Framebuffer para debug gráfico
- [ ] **Teclado PS/2:** Captura de scancodes y traducción a caracteres
- [ ] **Mouse PS/2:** Soporte para puntero en pantalla
- [ ] **Temporizador (PIT/APIC):** Control de tiempo para procesos

### Fase 3: Kernel Moderno y Multitarea
- [ ] **Heap Manager:** `kmalloc()` y `kfree()` para el kernel
- [ ] **Scheduler Round-Robin:** Multitarea preemptiva
- [ ] **VFS (Virtual File System):** Abstracción de sistemas de archivos
- [ ] **Initrd:** Sistema de archivos en RAM (sin drivers de disco complejos)

### Fase 3.5: Networking & Storage
- [ ] **Driver NIC:** Primer contacto con la red (Intel e1000)
- [ ] **Integración lwIP:** Stack TCP/IP ligero para networking
- [ ] **Soporte FAT32:** Lectura básica de archivos

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
- [ ] **Motor de Dibujo 2D:** Primitivas: píxeles, líneas, rectángulos, fuentes bitmap
- [ ] **Double Buffering:** Renderizado sin parpadeo
- [ ] **Event Loop:** Sistema de mensajes (mouse, teclado → ventanas)
- [ ] **Gestor de Ventanas (Compositor):** Superposición de ventanas con foco
- [ ] **Primera App:** Calculadora o Monitor de Sistema con GUI nativa

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
┌──────────────────────────────────────────────────┐
│ 1. BIOS carga el MBR (sector 0) en 0x7C00       │
│    └─▶ Stage 1 ejecuta                           │
│                                                  │
│ 2. Stage 1: Habilita A20, carga Stage 2 + Kernel │
│    └─▶ Salta a Stage 2 (0x7E00)                  │
│                                                  │
│ 3. Stage 2: Verifica Long Mode, configura GDT    │
│    └─▶ Entra en Modo Protegido (32-bit)          │
│                                                  │
│ 4. Stage 2: Configura paginación (PML4/PDPT/PD)  │
│    └─▶ Activa PAE + EFER LME                     │
│                                                  │
│ 5. Stage 2: Activa paginación, carga GDT64       │
│    └─▶ Entra en Long Mode (64-bit)               │
│                                                  │
│ 6. Long Mode: Salta a kmain() en 0x10000         │
│    └─▶ ¡Ether-Core de éterOS ejecutándose!       │
└──────────────────────────────────────────────────┘
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

Proyecto de desarrollo de sistemas operativos — Nueva Era.
