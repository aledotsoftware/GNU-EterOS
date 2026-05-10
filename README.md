# 🌌 éterOS: El Sistema Operativo Universal (vv0.2.0 "Genesis SMP")

éterOS es un sistema operativo de **Nueva Era**, desarrollado desde el **"cero absoluto" (Bare-Metal)** en C y ensamblador. Diseñado con una arquitectura de **Kernel Híbrido** y una **HAL (Hardware Abstraction Layer)** universal, su objetivo es ser el tejido conectivo entre microcontroladores IoT, dispositivos móviles ARM64 y potentes servidores x86_64.

Este documento es el manifiesto técnico y la única fuente de verdad sobre la arquitectura, funcionamiento y visión de éterOS.

---

## 📖 Tabla de Contenidos
1. [Filosofía y Visión](#1-filosofía-y-visión)
2. [Arquitectura General (Ether-Core)](#2-arquitectura-general-ether-core)
3. [Secuencia de Arranque y Hardware](#3-secuencia-de-arranque-y-hardware)
4. [Gestión de Memoria (MMU)](#4-gestión-de-memoria-mmu)
5. [Planificador, SMP y Sincronización](#5-planificador-smp-y-sincronización)
6. [Sistema de Archivos Virtual (VFS)](#6-sistema-de-archivos-virtual-vfs)
7. [Networking y Stack TCP/IP](#7-networking-y-stack-tcpip)
8. [Eterland: Servidor Gráfico y Motor Omni](#8-eterland-servidor-gráfico-y-motor-omni)
9. [Espacio de Usuario y Compatibilidad (ACS)](#9-espacio-de-usuario-y-compatibilidad-acs)
10. [EterStore: Gestión de Paquetes](#10-eterstore-gestión-de-paquetes)
11. [Guía de Soporte ARM64 (Rockchip RK3566)](#11-guía-de-soporte-arm64-rockchip-rk3566)
12. [Compilación y Toolchain](#12-compilación-y-toolchain)
13. [Hoja de Ruta y Faltantes Críticos](#13-hoja-de-ruta-y-faltantes-críticos)

---

## 1. Filosofía y Visión

éterOS no es un clon de UNIX; es una reinvención. Se construye sobre tres pilares:
*   **Ether-Core (Arquitectura Híbrida):** Combina la estabilidad de un microkernel (aislando drivers en espacio de usuario) con el rendimiento de un núcleo monolítico para operaciones críticas.
*   **Fluidez Nativa:** Ignoramos el lastre de los 90. Diseñado para hardware moderno: **UEFI/GOP**, **NVMe**, **Multicore (SMP)** y renderizado acelerado.
*   **Aero-Minimalismo:** Una experiencia visual basada en vectores, transparencias (**Glassmorphism**) y una interfaz fluida denominada **Flux UI**.

### Sistema de Tiers (Portabilidad)
La HAL permite que éterOS escale según el hardware:
*   **Tier 1 (Micro):** IoT, Sensores (ARM Cortex-M, RISC-V32). Sin MMU.
*   **Tier 2 (Core):** Tablets, RPi (AArch64). MMU + Paging.
*   **Tier 3 (Full):** PCs, Servidores (x86_64). MMU + Ring 0-3 + SMP.

---

## 2. Arquitectura General (Ether-Core)

El sistema está dividido en capas estrictas para garantizar la seguridad y la abstracción:

```text
╔════════════════════════════════════════════════════════════════════════════╗
║                        ESPACIO DE USUARIO (Ring 3)                         ║
║   ┌──────────┐  ┌──────────┐  ┌──────────────┐  ┌────────────────────┐     ║
║   │  sh.elf  │  │ test.elf │  │ exec_test.elf│  │  Aplicaciones ELF  │     ║
║   └────┬─────┘  └────┬─────┘  └──────┬───────┘  └──────────┬─────────┘     ║
║   ┌────┴─────────────┴───────────────┴─────────────────────┴──────────┐    ║
║   │                    libc (Mini-LibC POSIX)                         │    ║
║   └───────────────────────────┬───────────────────────────────────────┘    ║
║                        syscall (INT/SYSCALL)                               ║
╠═══════════════════════════════╪════════════════════════════════════════════╣
║                     ESPACIO DEL KERNEL (Ring 0)                            ║
║ ┌────────────────────────────────────────────────────────────────────────┐ ║
║ │  INTERFAZ DE SYSCALLS (~70 Syscalls Linux-Compatible ABI)              │ ║
║ └──────────────────────────────┬─────────────────────────────────────────┘ ║
║ ┌──────────┐  ┌─────────┐  ┌───────────┐  ┌──────────────────┐             ║
║ │SCHEDULER │  │  SHELL  │  │  NETWORK  │  │  APLICACIONES    │             ║
║ │(task.c)  │  │(shell/) │  │ (net/)    │  │  INTERNAS (apps/)│             ║
║ └────┬─────┘  └────┬────┘  └─────┬─────┘  └────────┬─────────┘             ║
║ ┌────┴──────────────┴───────┴────────┴─────────────────┴──────────┐        ║
║ │  MEMORY MGR (mm/)  │  FILESYSTEM (VFS) (fs/)  │   CRYPTO (crypto/)     │ ║
║ └─────────┬────────────────────┬─────────────────────────────────────────┘ ║
║ ┌─────────┴────────────────────┴─────────────────────────────────────────┐ ║
║ │            HARDWARE ABSTRACTION LAYER (HAL) (include/hal.h)            │ ║
║ └──────────────────────────────┬─────────────────────────────────────────┘ ║
║ ┌──────────────────────────────┼─────────────────────────────────────────┐ ║
║ │  DRIVERS (Video, Serial, Input, Net, Disk, Timer, PCI, RTC, ACPI)      │ ║
║ └──────────────────────────────┬─────────────────────────────────────────┘ ║
║ ┌──────────────────────────────┼─────────────────────────────────────────┐ ║
║ │              CÓDIGO ESPECÍFICO DE ARQUITECTURA                         │ ║
║ │          (x86_64: gdt, idt, pic, apic, smp, context_switch)            │ ║
║ └────────────────────────────────────────────────────────────────────────┘ ║
╚════════════════════════════════════════════════════════════════════════════╝
```

---

## 3. Secuencia de Arranque y Hardware

### El Salto al Modo Largo (x86_64)
éterOS despierta en un entorno de **64 bits puro**:
1.  **BIOS POST → MBR (boot.asm):** El bootloader de 2 etapas carga el kernel (128 sectores) y el Initrd.
2.  **Detección:** Se consulta la memoria vía E820, se activa la puerta A20 y se detecta el framebuffer VBE.
3.  **Transición:** Se configuran las tablas de paginación iniciales (Identity Mapping de 4GB), GDT de 64 bits y se activa PAE + EFER.LME.
4.  **kmain():** El núcleo toma el control en `0x10000`.

### Inicialización de Hardware (`hal_init`)
1.  **Interrupts (IDT/PIC):** Se remapean las IRQs (0x20-0x2F). Se configuran 256 vectores para excepciones, IRQs e IPIs (Inter-Processor Interrupts).
2.  **GDT & TSS:** Configuración per-CPU para el manejo de stacks durante interrupciones desde Ring 3.
3.  **Timer:** PIT (Programmable Interval Timer) a 100Hz para preempción.
4.  **ACPI & APIC:** Parseo de tablas RSDP/MADT para descubrir la topología de la CPU (Cores).

---

## 4. Gestión de Memoria (MMU)

La memoria se gestiona en tres niveles estrictos:

### 1. Physical Memory Manager (PMM)
*   **Estructura:** Bitmap (1 bit por cada frame de 4KB). ~4KB de bitmap por cada 128MB de RAM.
*   **Algoritmo:** **Next-Fit** con un puntero "rover" y escaneo por palabras (Word-Scanning) para lograr O(1) amortizado.
*   **Features:** Soporta conteo de referencias (`pmm_ref_page`) crucial para la implementación de Copy-on-Write.

### 2. Virtual Memory Manager (VMM)
*   **Arquitectura:** Paginación de 4 niveles (PML4 → PDPT → PD → PT) propia de x86_64.
*   **Copy-on-Write (CoW):** Al hacer `fork()`, no se copia la memoria física. Las páginas se marcan como "Read-Only". Un Page Fault en escritura duplica la página al vuelo.
*   **TLB Shootdown:** En entornos SMP, modificar una tabla de paginación requiere enviar una IPI (Inter-Processor Interrupt) a los demás núcleos para que invaliden su TLB (Translation Lookaside Buffer).

### 3. Heap Dinámico (`kmalloc`)
*   **Algoritmo:** Next-Fit con listas doblemente enlazadas y **Coalescing** automático (fusión de bloques libres adyacentes).
*   **Seguridad:** Implementa *canary checks* (magic numbers) para detectar desbordamientos de buffer o dobles liberaciones.

#### Mapa de Memoria Virtual (x86_64)
| Rango de Dirección | Contenido |
|--------------------|-----------|
| `0x0000000000000000` | Inicio (Identity mapped los primeros 4GB por el bootloader) |
| `0x0000000000100000` | Kernel Heap Start |
| `0x00000v0.2.0000000` | User Code (Ring 3 ELF Load Address) |
| `0x0000000300000000` | User Stack (Ring 3) |
| `0xFFFF800000000000` | Kernel High-Half (Reservado para diseño futuro) |

---

## 5. Planificador, SMP y Sincronización

### Scheduler (Round-Robin Preemptivo)
El planificador de éterOS escala para múltiples núcleos.
*   **Estados:** `READY`, `RUNNING`, `SLEEPING`, `BLOCKED`, `ZOMBIE`.
*   **Context Switch:** Ultra-optimizado en ensamblador (`context_switch.asm`). Guarda RSP, cambia espacio de direcciones (CR3) si es necesario, y salta.
*   **Tick:** Impulsado por el timer PIT a 100Hz (10ms por quantum).

### SMP (Symmetric Multiprocessing)
éterOS no es mononúcleo.
*   **AP Booting:** El procesador principal (BSP) copia un trampolín de 16 bits a memoria baja y envía señales `INIT` y `STARTUP` (SIPI) a los Application Processors (APs).
*   **Per-CPU Data:** Utiliza el registro `GS_BASE` de x86_64 para almacenar punteros al contexto actual de cada CPU de forma aislada, evitando locks globales masivos.

### Primitivas de Sincronización
1.  **Spinlocks:** Bloqueos activos para proteger estructuras críticas del kernel (Heap, VFS, PMM) del acceso concurrente entre núcleos.
2.  **Semáforos:** Para suspender hilos a la espera de I/O (ej. red, disco).
3.  **Futexes (Fast Userspace Mutexes):** La joya de la corona para compatibilidad Linux. Implementados en `futex.c` con colas de espera en Ring 0 para que la Mini-LibC gestione hilos POSIX sin overhead.

---

## 6. Sistema de Archivos Virtual (VFS)

El VFS abstrae el almacenamiento tras una API unificada (`read_fs`, `write_fs`, `open_fs`, `mount`). 

**Árbol de Montajes:**
*   `/` **(Initrd):** Initial Ramdisk cargado por el bootloader. Contiene `sh.elf`, binarios de prueba y assets gráficos. Solo lectura.
*   `/dev/` **(DevFS):** Nodos virtuales. Implementados: `null`, `zero`, `tty`, `random` (PRNG), `urandom`, `input` (Ring buffer de eventos de teclado/mouse).
*   `/proc/` **(ProcFS):** Estado del núcleo. `/proc/uptime`, `/proc/version`, `/proc/meminfo`.
*   `/mnt/` **(FAT32):** Soporte estable para lectura/escritura de particiones FAT32 (nombres 8.3) con soporte para offsets de partición MBR.
*   `/data/` **(JFS):** Prototipo de Journaling File System en memoria.

---

## 7. Networking y Stack TCP/IP

éterOS posee capacidades de red avanzadas a través de su driver nativo **Intel PRO/1000 (e1000)** (descubierto vía escaneo PCI).

### Integración lwIP 2.2.0
El kernel integra el stack industrial **lwIP** (`kernel/net/lwip_port`).
*   **DHCP:** Negociación automática de IP al inicio (`network_task`).
*   **Sockets:** Soporte para Sockets BSD a través de la capa de syscalls.
*   **DNS & TCP:** Soporte completo. El comando `wget` en la shell permite descargar archivos vía HTTP.

---

## 8. Eterland: Servidor Gráfico y Motor Omni

**Eterland** es el compositor gráfico diseñado bajo el principio de **Cero Copia (Zero-Copy)**.

### Arquitectura de Composición
1.  **Shared Buffers (SHM):** Las aplicaciones en Ring 3 piden un buffer. El kernel lo mapea en el proceso y en el compositor (`sys_mmap(MAP_SHARED)`).
2.  **IPC:** La app dibuja (CPU/GPU) y notifica al compositor vía Unix Domain Sockets.
3.  **Motor Omni v2.0:** El compositor mezcla las ventanas aplicando:
    *   **Alpha Blending (256 niveles)** para Glassmorphism.
    *   **Dirty Region Tracking:** Solo actualiza los rectángulos del framebuffer que sufrieron cambios.
    *   **Fast-Path Geometry:** Primitivas que escriben directamente en el buffer de video LFB (Linear Framebuffer) obtenido vía VBE/GOP.

---

## 9. Espacio de Usuario y Compatibilidad (ACS)

La transición a Ring 3 se realiza configurando los segmentos de usuario en la GDT y ejecutando la instrucción `SYSRET`.

### Cargador ELF64
El kernel parsea cabeceras ELF, mapea los segmentos `PT_LOAD` en memoria virtual y configura el heap del proceso (gestionado vía la syscall `brk`).

### ACS: Aether-Linux Subsystem
El objetivo es ejecutar binarios ELF de Linux sin modificarlos.
*   **Multiplexor:** El manejador `syscall_entry.asm` intercepta la instrucción `syscall` y traduce los registros estándar de Linux (RAX=id, RDI, RSI, RDX, R10, R8, R9=args) a funciones nativas.
*   **Tabla de Syscalls Implementadas (~70):**
    *   *I/O:* `read`, `write`, `open`, `close`, `lseek`, `ioctl`, `readv`, `writev`, `dup2`, `pipe`.
    *   *Procesos:* `fork`, `execve`, `exit`, `wait4`, `getpid`, `kill`.
    *   *Memoria:* `mmap`, `brk`, `mprotect` (stub), `munmap`.
    *   *Sincronización:* `futex`, `arch_prctl` (esencial para Thread Local Storage `FS/GS base`).
    *   *FS:* `stat`, `fstat`, `mkdir`, `unlink`.

### Mini-LibC (POSIX)
Para aplicaciones nativas de éterOS, proveemos una librería en `userspace/libc/` con soporte para:
*   Asignación dinámica (`malloc`/`free` usando bloques sobre `brk`).
*   Manipulación de cadenas (`string.h`).
*   Señales (`signal.h` con colas de interrupción diferida en el scheduler).

---

## 10. EterStore: Gestión de Paquetes

La infraestructura para instalar software se basa en dos fases:
1.  **Fase de Adopción:** Herramientas CLI capaces de extraer paquetes Debian (`.deb`) mapeando sus dependencias mediante capas de traducción.
2.  **Fase Nativa (Formato `.epk`):** Paquetes EterPackage. Archivos SquashFS/Zstd de solo lectura que el kernel monta dinámicamente en tiempo de ejecución, garantizando desinstalaciones limpias y sandboxing.

---

## 11. Guía de Soporte ARM64 (Rockchip RK3566)

éterOS está portando su HAL a la arquitectura AArch64 para correr en tablets y SBCs.
*   **Estado:** Experimental (Bootea, inicializa UART, Serial Debug 1.5M baudios. Sin display aún).
*   **Flasheo:**
    1. Compilar cross-compiler: `.\build.ps1 -Target all -Arch aarch64`
    2. Conectar dispositivo en modo LOADER/MASKROM.
    3. Usar **RKDevTool** para escribir `build/aarch64/kernel.bin` en la partición `boot`.

---

## 12. Compilación y Toolchain

El sistema se puede compilar en Windows (PowerShell) o Linux (WSL).
**Requisitos:** `x86_64-elf-gcc`, `nasm`, `x86_64-elf-ld`, `qemu-system-x86_64`, `xorriso`.

### Entorno Windows (Nativo)
El script `build.ps1` orquesta todo el proceso:
```powershell
.\build.ps1 -Target all     # Compila boot, kernel, userspace e initrd
.\build.ps1 -Target run     # Lanza QEMU con red (e1000) y Serial COM1
.\build.ps1 -Target vbox    # Empaqueta en un disco VDI para VirtualBox
.\build.ps1 -Target clean   # Limpia el directorio de build
```

### Entorno Linux / WSL
```bash
make all      # Genera build/eteros.img
make run      # Ejecuta QEMU
make iso      # Genera un ISO booteable para grabar en USB
```

---

## 13. Hoja de Ruta y Faltantes Críticos

Aunque éterOS vv0.2.0 es altamente funcional, las siguientes áreas son la prioridad actual de desarrollo:

🔴 **Críticos (Blockers):**
1.  **Cargador de Librerías Dinámicas (`.so`):** Actualmente el loader ELF asume compilación estática. Se requiere soporte para `PT_DYNAMIC` para avanzar hacia el port completo de la libc GNU.
2.  **Aether-Droid (Binder IPC):** Implementación de transacciones y enrutamiento en el driver `/dev/binder` dentro del kernel para habilitar comunicación inter-proceso estilo Android.
3.  **Abstracción DRM/KMS:** El motor Omni v2.0 opera sobre LFB (Linear Framebuffer). Se necesita una abstracción DRM (Direct Rendering Manager) y KMS (Kernel Mode Setting) para un entorno de escritorio completo.

🟡 **Mejoras Arquitectónicas (Largo Plazo):**
4.  **Compatibilidad Syscalls Linux:** Expandir la compatibilidad de POSIX PTY ioctls y soporte más robusto para `fork/exec/clone` que soporte la terminal GNU real (bash/coreutils).
5.  **Sistema Multiusuario Completo:** Si bien ya se parsean `/etc/shadow` y `/etc/passwd` y hay UIDs por proceso, se requiere un binario `/bin/login` que coordine la entrada, genere tokens de sesión y asigne `/dev/ttyX`.

---

## 📜 Licencia y Créditos

© 2025 **Tudex Networks**. Desarrollado por el equipo de éterOS.
Este proyecto es una obra de ingeniería dedicada a la exploración de nuevos paradigmas en sistemas operativos, empujando los límites del silicio.

---
*"El éter es la plataforma; éterOS es la inteligencia que la habita."*
