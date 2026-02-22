# éterOS — Arquitectura Completa del Sistema

**Versión:** 0.2.0 "Genesis SMP"  
**Copyright:** (c) 2026 Tudex Networks. All rights reserved.  
**Última actualización:** 2026-02-20  

---

## 1. Visión General

éterOS es un sistema operativo educativo/experimental escrito en C y ensamblador x86_64,
diseñado con una **HAL (Hardware Abstraction Layer)** que permite portabilidad a múltiples
arquitecturas. El kernel es monolítico con soporte para:

- **Multitarea preemptiva** (Round-Robin con SMP)
- **Memoria virtual** (Paginación 4-nivel x86_64 con Copy-on-Write)
- **Sistema de archivos virtual** (VFS con múltiples backends)
- **Networking** (lwIP stack completo con DHCP, DNS, TCP)
- **Espacio de usuario** (Ring 3 con ELF loader y syscalls Linux-compatible)

---

## 2. Diagrama de Capas del Sistema

```
╔════════════════════════════════════════════════════════════════════════════╗
║                        ESPACIO DE USUARIO (Ring 3)                         ║
║                                                                            ║
║   ┌──────────┐  ┌──────────┐  ┌──────────────┐  ┌────────────────────┐     ║
║   │  sh.elf  │  │ test.elf │  │ exec_test.elf│  │  Aplicaciones ELF  │     ║
║   └────┬─────┘  └────┬─────┘  └──────┬───────┘  └──────────┬─────────┘     ║
║        │             │               │                     │               ║
║   ┌────┴─────────────┴───────────────┴─────────────────────┴──────────┐    ║
║   │                    libc (Userspace Library)                       │    ║
║   │  string.c · stdio.c · stdlib.c · unistd.c · signal.c · crt0.asm   │    ║
║   │  errno.c · ctype.c · fcntl.c · sys/stat.c · sys/wait.c · etc.     │    ║
║   └───────────────────────────┬───────────────────────────────────────┘    ║
║                               │                                            ║
║                        syscall (INT/SYSCALL)                               ║
╠═══════════════════════════════╪════════════════════════════════════════════╣
║                               ▼                                            ║
║                     ESPACIO DEL KERNEL (Ring 0)                            ║
║                                                                            ║
║ ┌────────────────────────────────────────────────────────────────────────┐ ║
║ │                     INTERFAZ DE SYSCALLS                               │ ║
║ │  syscall_entry.asm → syscall_handler() → sys_read/write/open/fork/     │ ║
║ │  exec/pipe/socket/mmap/brk/kill/wait4/dup2/stat/ioctl/futex/...        │ ║
║ │                      (~70 syscalls Linux-compatible)                   │ ║
║ └──────────────────────────────┬─────────────────────────────────────────┘ ║
║                                │                                           ║
║ ┌──────────────────────────────┼─────────────────────────────────────────┐ ║
║ │              SUBSISTEMAS DEL KERNEL                                    │ ║
║ │                              │                                         │ ║
║ │  ┌──────────┐  ┌─────────┐  │  ┌───────────┐  ┌──────────────────┐     │ ║
║ │  │SCHEDULER │  │  SHELL  │  │  │  NETWORK  │  │  APLICACIONES    │     │ ║
║ │  │(task.c)  │  │(shell/) │  │  │ (net/)    │  │  INTERNAS (apps/)│     │ ║
║ │  └────┬─────┘  └────┬────┘  │  └─────┬─────┘  └────────┬─────────┘     │ ║
║ │       │              │       │        │                 │              │ ║
║ │  ┌────┴──────────────┴───────┴────────┴─────────────────┴──────────┐   │ ║
║ │  │                  SERVICIOS DEL KERNEL                           │   │ ║
║ │  │  klog.c · stdio.c · string.c · futex.c · sem.c · libgcc.c       │   │ ║
║ │  └────────────────────────────┬────────────────────────────────────┘   │ ║
║ │                               │                                        │ ║
║ │  ┌─────────────┐  ┌──────────┴──────────┐  ┌──────────────────────┐    │ ║
║ │  │ MEMORY MGR  │  │ FILESYSTEM (VFS)    │  │   CRYPTO             │    │ ║
║ │  │ (mm/)       │  │ (fs/)               │  │   (crypto/)          │    │ ║
║ │  └──────┬──────┘  └──────────┬──────────┘  └──────────────────────┘    │ ║
║ │         │                    │                                         │ ║
║ └─────────┼────────────────────┼─────────────────────────────────────────┘ ║
║           │                    │                                           ║
║ ┌─────────┴────────────────────┴─────────────────────────────────────────┐ ║
║ │            HARDWARE ABSTRACTION LAYER (HAL)                            │ ║
║ │        hal.h (API) → kernel/arch/<arch>/hal_impl.c                     │ ║
║ └──────────────────────────────┬─────────────────────────────────────────┘ ║
║                                │                                           ║
║ ┌──────────────────────────────┼─────────────────────────────────────────┐ ║
║ │                     DRIVERS DE DISPOSITIVOS                            │ ║
║ │   video/   serial/   input/   net/   disk/   timer/   pci/   rtc/      │ ║
║ └──────────────────────────────┬─────────────────────────────────────────┘ ║
║                                │                                           ║
║ ┌──────────────────────────────┼─────────────────────────────────────────┐ ║
║ │              CÓDIGO ESPECÍFICO DE ARQUITECTURA                         │ ║
║ │                   kernel/arch/x86_64/                                  │ ║
║ │   gdt · idt · pic · apic · smp · context_switch · syscall_entry        │ ║
║ │   trampoline · user_mode · pat · acpi · interrupts                     │ ║
║ └──────────────────────────────┬─────────────────────────────────────────┘ ║
║                                │                                           ║
║ ┌──────────────────────────────┼─────────────────────────────────────────┐ ║
║ │                         BOOTLOADER                                     │ ║
║ │              boot/x86_64/boot.asm (+ UEFI boot)                        │ ║
║ └──────────────────────────────┬─────────────────────────────────────────┘ ║
╠════════════════════════════════╪══════════════════════════════════════════─╣
║                                ▼                                           ║
║                        HARDWARE FÍSICO                                     ║
║                  CPU · RAM · Disco · NIC · Display                         ║
╚════════════════════════════════════════════════════════════════════════════╝
```

---

## 3. Módulos Detallados

### 3.1. Bootloader (`boot/`)

```
boot/
├── x86_64/
│   ├── boot.asm          ← Bootloader principal (29KB)
│   │                       • MBR stage 1 → stage 2
│   │                       • Configura A20, GDT temporal
│   │                       • Detecta RAM (E820), VESA/VBE
│   │                       • Transición a Long Mode (64-bit)
│   │                       • Paginación inicial (Identity Map 4GB)
│   │                       • Carga initrd
│   │                       • Salta a kmain()
│   ├── boot_pxe.asm      ← Boot por PXE (red)
│   ├── linker.ld         ← Script de enlace del kernel
│   └── uefi/             ← Boot UEFI (alternativo)
├── aarch64/              ← Boot ARM64 (stub)
├── i386/                 ← Boot x86 32-bit (legacy)
└── common/               ← Headers compartidos
```

**Flujo de Boot:**
```
BIOS/UEFI → boot.asm → {A20, GDT, E820, Long Mode, Paging} → kmain()
```

---

### 3.2. HAL — Hardware Abstraction Layer (`include/hal.h`)

La HAL es el corazón de la portabilidad. Define una **API común** que toda arquitectura debe implementar:

```
┌──────────────────────────────────────────────────────────────┐
│                     HAL API (hal.h)                          │
│                                                              │
│  hal_init()              ← Inicializa todo el hardware       │
│  hal_interrupts_enable/disable()                             │
│  hal_irq_install()       ← Registrar handler de IRQ          │
│  hal_cpu_halt()          ← HLT (esperar interrupción)        │
│  hal_cpu_reset()         ← Reinicio del sistema              │
│  hal_console_init/putchar/write/clear()                      │
│  hal_input_init/getchar/available()                          │
│  hal_timer_init(hz) / hal_timer_ticks()                      │
│  hal_mem_map/unmap/get_phys()    ← MMU                       │
│  hal_debug_putchar/write()       ← Serial debug              │
└──────────────────────────────────────────────────────────────┘
                              │
          ┌───────────────────┼───────────────────┐
          ▼                   ▼                   ▼
┌──────────────┐  ┌───────────────┐  ┌─────────────────┐
│   x86_64     │  │   aarch64     │  │  arm-cortex-m   │
│  hal_impl.c  │  │  hal_impl.c   │  │  hal_impl.c     │
│  (172 líneas)│  │  (stub)       │  │  (stub)         │
└──────────────┘  └───────────────┘  └─────────────────┘
                                     ┌──────────────────┐
                                     │  riscv64 / xtensa│
                                     │  hal_impl.c      │
                                     └──────────────────┘
```

**Sistema de Tiers:**
| Tier | Nombre | Características | Arquitecturas |
|------|--------|----------------|---------------|
| 1 | éterOS-Micro | Shell + UART, sin display, sin MMU | ARM Cortex-M, AVR, RISC-V32, Xtensa |
| 2 | éterOS-Core | + Display, + MM, + IPC | AArch64, RISC-V64 |
| 3 | éterOS-Full | + Filesystem, + Networking, + GUI | x86_64 |

---

### 3.3. Código Específico x86_64 (`kernel/arch/x86_64/`)

```
kernel/arch/x86_64/
├── hal_impl.c             ← Implementación de HAL para x86_64
├── gdt.c                  ← Global Descriptor Table (+ TSS per-CPU)
├── gdt_flush.asm          ← Recarga de GDT en ASM
├── idt.c                  ← Interrupt Descriptor Table (exceptions + IRQs)
├── interrupts.asm         ← Stubs ISR en ensamblador
├── pic.c                  ← Programmable Interrupt Controller (8259A)
├── apic.c                 ← Advanced PIC (Local APIC)
├── acpi.c                 ← ACPI parser (RSDP, MADT, CPU topology)
├── smp.c                  ← Symmetric Multiprocessing (Boot APs)
├── trampoline.asm         ← Código AP boot (Real→Protected→Long)
├── smp_trampoline_wrapper.asm
├── pat.c                  ← Page Attribute Table (Write-Combining)
├── syscall.c              ← Handler de syscalls (~1300 líneas, ~70 syscalls)
├── syscall_entry.asm      ← Entry point MSR LSTAR
├── context_switch.asm     ← Cambio de contexto (save/restore regs)
├── user_mode.asm          ← SYSRET a Ring 3
└── user_payload.asm       ← Payload de prueba Ring 3
```

**Secuencia de inicialización HAL x86_64:**
```
hal_init()
    ├── hal_console_init()    → VGA Terminal + Serial COM1
    ├── gdt_init()            → GDT con TSS
    ├── pic_init()            → Remap IRQs (0x20-0x2F)
    ├── idt_init()            → 256 vectores (Exceptions + IRQs + IPI)
    ├── syscall_init()        → MSR LSTAR/STAR/FMASK
    ├── input_init()          → Input event system
    ├── hal_input_init()      → PS/2 Keyboard + IRQ1
    ├── hal_timer_init(100)   → PIT a 100Hz + IRQ0
    └── hal_interrupts_enable() → STI
```

---

### 3.4. Gestión de Memoria (`kernel/mm/`)

```
┌──────────────────────────────────────────────────────────────────────┐
│                   SISTEMA DE GESTIÓN DE MEMORIA                      │
│                                                                      │
│  ┌─────────────────────────────────────────────────────────────────┐ │
│  │                    Heap Allocator (heap.c)                      │ │
│  │  Algoritmo: Next-Fit + Coalescing                               │ │
│  │  API: kmalloc() · kfree() · kcalloc() · krealloc()              │ │
│  │  Protección: Spinlock SMP · Canary checks                       │ │
│  │  Lista doblemente enlazada de bloques                           │ │
│  └────────────────────────────┬────────────────────────────────────┘ │
│                               │ usa                                  │
│  ┌────────────────────────────▼────────────────────────────────────┐ │
│  │              Virtual Memory Manager (vmm.c)                     │ │
│  │  Paginación 4 niveles: PML4 → PDPT → PD → PT                    │ │
│  │  API:                                                           │ │
│  │    vmm_map_page() · vmm_unmap_page() · vmm_virt_to_phys()       │ │
│  │    vmm_clone_pml4() (Copy-on-Write para fork)                   │ │
│  │    vmm_handle_page_fault() (resuelve CoW)                       │ │
│  │    vmm_verify_user_access() · vmm_validate_user_ptr()           │ │
│  │  TLB Shootdown SMP (IPI-based)                                  │ │
│  └────────────────────────────┬────────────────────────────────────┘ │
│                               │ usa                                  │
│  ┌────────────────────────────▼────────────────────────────────────┐ │
│  │           Physical Memory Manager (pmm.c)                       │ │
│  │  Bitmap de páginas físicas (4KB por frame)                      │ │
│  │  Estrategia: Next-Fit con rover                                 │ │
│  │  API:                                                           │ │
│  │    pmm_init() (parsea E820)                                     │ │
│  │    pmm_alloc_page() · pmm_free_page()                           │ │
│  │    pmm_ref_page() · pmm_unref_page() (reference counting)       │ │
│  │    pmm_get_total/free/used_ram()                                │ │
│  └─────────────────────────────────────────────────────────────────┘ │
└──────────────────────────────────────────────────────────────────────┘
```

**Mapa de memoria virtual:**
```
0x0000000000000000  ┬── Espacio de usuario (identidad)
                    │   identity mapped primeros 4GB por bootloader
0x0000000100000000  │
                    │
0x0000000200000000  ├── User code (ELF load address)
                    │
0x0000000300000000  ├── User stack
                    │
                    │   ... espacio libre ...
                    │
0xFFFF800000000000  ├── Kernel high-half (futuro, no implementado aún)
```

---

### 3.5. Scheduler y Procesos (`kernel/task.c`)

```
┌──────────────────────────────────────────────────────────────┐
│              SCHEDULER (Round-Robin Preemptivo)              │
│                                                              │
│  Estados de Tarea:                                           │
│    TASK_READY → TASK_RUNNING → TASK_SLEEPING → TASK_DEAD     │
│                              → TASK_BLOCKED                  │
│                              → TASK_ZOMBIE                   │
│                                                              │
│  Funciones principales:                                      │
│    scheduler_init()    ← Crea tarea idle (PID 0)             │
│    task_create()       ← Crea nueva tarea kernel             │
│    task_fork()         ← fork() con CoW                      │
│    task_exec()         ← execve() (carga ELF)                │
│    task_waitpid()      ← wait4()                             │
│    schedule()          ← Selecciona siguiente tarea          │
│    task_yield()        ← Cede CPU voluntariamente            │
│    task_sleep(ms)      ← Sleep con timer                     │
│    task_exit()         ← Termina tarea actual                │
│    task_kill()         ← Mata tarea por PID                  │
│                                                              │
│  Cada tarea tiene:                                           │
│    • Stack de kernel propio                                  │
│    • PML4 propio (espacio de direcciones)                    │
│    • Tabla de file descriptors (MAX_FDS)                     │
│    • PID, PPID, nombre                                       │
│    • Registros guardados (contexto)                          │
│    • Puntero a semáforo de espera                            │
│    • brk (heap boundary del userspace)                       │
│    • Estado, ticks consumidos                                │
│                                                              │
│  Cambio de contexto:                                         │
│    IRQ0 (Timer) → schedule() → context_switch.asm            │
│                   (guarda RSP, carga nuevo RSP, ret)         │
│                                                              │
│  SMP:                                                        │
│    • task_init_ap() para Application Processors              │
│    • Spinlock en el scheduler                                │
│    • Per-CPU kernel stack en TSS                             │
└──────────────────────────────────────────────────────────────┘
```

---

### 3.6. Sistema de Archivos (`kernel/fs/`)

```
┌──────────────────────────────────────────────────────────────────────┐
│                    VIRTUAL FILE SYSTEM (VFS)                         │
│                                                                      │
│  API unificada:                                                      │
│    read_fs() · write_fs() · open_fs() · close_fs()                   │
│    readdir_fs() · finddir_fs() · create_fs() · mkdir_fs()            │
│    unlink_fs() · ioctl_fs() · vfs_mount() · vfs_lookup()             │
│                                                                      │
│  Soporte de montaje:                                                 │
│    Mount points via lista enlazada (inode+impl matching)             │
│    Spinlock per-node para protección SMP                             │
│                                                                      │
│  Árbol de filesystems montados:                                      │
│                                                                      │
│    /                    ← initrd (Initial Ramdisk)                   │
│    ├── sh.elf           ← Shell userspace                            │
│    ├── test.elf         ← Test program                               │
│    ├── dev/             ← DevFS (dispositivos virtuales)             │
│    │   ├── null         ← /dev/null                                  │
│    │   ├── zero         ← /dev/zero                                  │
│    │   ├── tty          ← /dev/tty (terminal)                        │
│    │   ├── random       ← /dev/random (xorshift + TSC)               │
│    │   ├── urandom      ← /dev/urandom                               │
│    │   └── input        ← /dev/input (eventos de entrada)            │
│    ├── proc/            ← ProcFS (información del sistema)           │
│    │   ├── version      ← eterOS version string                      │
│    │   ├── uptime       ← Uptime en segundos                         │
│    │   └── meminfo      ← Memoria total/free/used                    │
│    └── data/            ← JFS (Journaling File System)               │
│                                                                      │
│  Backends de FS:                                                     │
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐    │
│  │  initrd  │ │  FAT32   │ │  DevFS   │ │  ProcFS  │ │   JFS    │    │
│  │(initrd.c)│ │(fat32.c) │ │(devfs.c) │ │(procfs.c)│ │ (jfs.c)  │    │
│  │ RAM-based│ │ Disk I/O │ │ Virtual  │ │ Virtual  │ │ Journaled│    │
│  │ Read-Only│ │ Read/Writ│ │ En-Memory│ │ En-Memory│ │ 4MB disk │    │
│  └──────────┘ └──────────┘ └──────────┘ └──────────┘ └──────────┘    │
│                                                                      │
│  Soporte adicional:                                                  │
│  ┌──────────────┐   ┌───────────────┐                                │
│  │ Block Cache  │   │  ELF Loader   │                                │
│  │ (bcache.c)   │   │  (elf.c)      │                                │
│  │ Cache de     │   │  Carga ELF64  │                                │
│  │ sectores     │   │  a memoria    │                                │
│  └──────────────┘   └───────────────┘                                │
└──────────────────────────────────────────────────────────────────────┘
```

---

### 3.7. Networking (`kernel/net/`)

```
┌─────────────────────────────────────────────────────────────────────┐
│                      STACK DE RED                                   │
│                                                                     │
│  ┌─────────────────────────────────────────────────────┐            │
│  │            Capa de Aplicación                       │            │
│  │  wget.c (HTTP GET) · raw_tcp_get() (compat.c)       │            │
│  └────────────────────────┬────────────────────────────┘            │
│                           │                                         │
│  ┌────────────────────────▼────────────────────────────┐            │
│  │            lwIP 2.2.0 (Stack completo)              │            │
│  │  TCP · UDP · IP · ICMP · DNS · DHCP                 │            │
│  │  lwip/    ← Código fuente de lwIP                   │            │
│  │  lwip_port/ ← Port de éterOS para lwIP              │            │
│  │    ethernetif.c  ← Interfaz NIC→lwIP                │            │
│  └────────────────────────┬────────────────────────────┘            │
│                           │                                         │
│  ┌────────────────────────▼────────────────────────────┐            │
│  │            Capa de Compatibilidad                   │            │
│  │  compat.c:                                          │            │
│  │    net_init() → Inicializa lwIP + DHCP              │            │
│  │    net_poll() → Procesa paquetes pendientes         │            │
│  │    raw_tcp_get() → HTTP client blocking             │            │
│  └────────────────────────┬────────────────────────────┘            │
│                           │                                         │
│  ┌────────────────────────▼─────────────────────────────┐           │
│  │            Stack nativo (core/)                      │           │
│  │  dhcp.c · dhcp_parser.c · ip_utils.c                 │           │
│  │  raw_tcp.c · tcp.c · stack.c · nic.c                 │           │
│  └────────────────────────┬─────────────────────────────┘           │
│                           │                                         │
│  ┌────────────────────────▼─────────────────────────────┐           │
│  │           Driver de Red                              │           │
│  │  e1000.c  ←  Intel E1000/E1000E NIC driver           │           │
│  │  Detectado via PCI (pci.c)                           │           │
│  └──────────────────────────────────────────────────────┘           │
│                                                                     │
│  Socket API (syscall.c):                                            │
│    sys_socket() · sys_connect()                                     │
│    socket_read_fs() · socket_write_fs()                             │
└─────────────────────────────────────────────────────────────────────┘
```

---

### 3.8. Drivers de Dispositivos (`kernel/drivers/`)

```
kernel/drivers/
│
├── video/
│   ├── vga.c              ← VGA text mode (80x25)
│   │                        • terminal_putchar/write_string/clear
│   │                        • Colores VGA, scroll
│   │                        • Hook serial para output dual
│   ├── framebuffer.c      ← Framebuffer lineal (VESA/VBE)
│   │                        • Resoluciones desde bootloader
│   │                        • terminal_switch_to_framebuffer()
│   └── font.c             ← Font bitmap embebido
│
├── input/
│   ├── keyboard.c         ← Controlador PS/2 Keyboard
│   │                        • Scancode set 1 → ASCII
│   │                        • Buffer circular de input
│   │                        • Teclas especiales (Shift, Ctrl, etc.)
│   ├── mouse.c            ← Controlador PS/2 Mouse
│   │                        • Movimiento X/Y + botones
│   │                        • Eventos al input subsystem
│   └── input.c            ← Sistema de eventos de entrada
│                            • Ring buffer de input_event_t
│                            • Lectura via /dev/input
│
├── serial/
│   └── serial.c           ← UART 16550 (COM1-COM4)
│                            • 115200 baud, 8N1
│                            • IRQ4, buffer de recepción
│                            • serial_read/write/putchar
│
├── timer/
│   └── pit.c              ← Programmable Interval Timer (8253/8254)
│                            • 100 Hz (10ms por tick)
│                            • timer_get_ticks(), timer_get_uptime_seconds()
│
├── net/
│   └── e1000.c            ← Intel E1000/E1000E NIC Driver
│                            • Detección via PCI
│                            • DMA ring buffers (TX/RX)
│                            • IRQ-driven reception
│
├── disk/
│   └── partition.c        ← Parser de tabla de particiones (MBR/GPT)
│                            • Lee primer sector (512 bytes)
│                            • Detecta FAT32 partitions
│
├── pci/
│   └── pci.c              ← PCI Bus Enumeration
│                            • Escaneo Bus/Device/Function
│                            • Config Space read/write
│                            • BAR mapping, IRQ routing
│
└── rtc/
    └── rtc.c              ← Real-Time Clock (CMOS)
                             • Lectura de fecha/hora
                             • NMI handling
```

---

### 3.9. Shell del Kernel (`kernel/shell/`)

```
┌──────────────────────────────────────────────────────────────┐
│                    SHELL INTERACTIVO                         │
│                                                              │
│  shell.c          ← Loop principal de entrada                │
│                     • Lee de teclado + serial                │
│                     • Historial con flechas ↑/↓              │
│                     • Ctrl+C (cancelar), Ctrl+L (clear)      │
│  commands.c       ← Dispatch de comandos                     │
│  cmd_system.c     ← Comandos de sistema:                     │
│                     reboot, clear, sysmon, ver, halt,        │
│                     cat, ls, cd, pwd, mkdir, echo, write     │
│  cmd_task.c       ← Comandos de procesos:                    │
│                     ps, kill, exec, fork_test, sleep         │
│  cmd_net.c        ← Comandos de red:                         │
│                     wget, ifconfig                           │
│  cmd_misc.c       ← Otros: date, uptime, uname               │
│  history.c        ← Buffer circular de historial             │
│  shell_internal.h ← Headers internos                         │
└──────────────────────────────────────────────────────────────┘
```

---

### 3.10. Aplicaciones Internas (`kernel/apps/`)

```
kernel/apps/
├── sysmon.c           ← EterMon (Monitor de Sistema)
│                        3 pantallas: Overview, CPU Details, Memory Map
│                        CPUID, vendor/brand string, feature flags
│                        Uso de RAM (PMM stats)
│
├── user_loader.c      ← Cargador de espacio de usuario
│                        • Setup FD 0/1/2 → /dev/tty
│                        • Carga sh.elf vía ELF loader
│                        • Configura stack de usuario (~0x300000000)
│                        • Salta a Ring 3 vía enter_user_mode()
│
├── santitravel.c      ← Aplicación demo custom
│
├── wget.c             ← HTTP client (usa raw_tcp_get)
│
└── doomgeneric/       ← Port de Doom (directorio stub)
```

---

### 3.11. Sincronización y IPC

```
┌──────────────────────────────────────────────────────────────┐
│               PRIMITIVAS DE SINCRONIZACIÓN                   │
│                                                              │
│  ┌────────────┐  ┌──────────────┐  ┌────────────────────┐    │
│  │  Spinlock  │  │  Semáforos   │  │   Futex (Fast      │    │
│  │  (lock.h)  │  │  (sem.c)     │  │   Userspace Mutex) │    │
│  │            │  │              │  │   (futex.c)        │    │
│  │ spin_lock()│  │ sem_init()   │  │                    │    │
│  │ spin_unlock│  │ sem_wait()   │  │ futex_wait()       │    │
│  │            │  │ sem_signal() │  │ futex_wake()       │    │
│  │ Usado por: │  │              │  │                    │    │
│  │ • Heap     │  │ Usado por:   │  │ 16 buckets hash    │    │
│  │ • VFS      │  │ • Network    │  │ Per-bucket spinlock│    │
│  │ • PMM      │  │ • Scheduler  │  │ Linux-compatible   │    │
│  │ • TLB flush│  │              │  │                    │    │
│  └────────────┘  └──────────────┘  └────────────────────┘    │
│                                                              │
│  ┌────────────────────────────────────────────────────────┐  │
│  │  Pipes (syscall.c)                                     │  │
│  │  • Buffer circular de 4KB                              │  │
│  │  • Spinlock protegido                                  │  │
│  │  • Soporta read/write/close                            │  │
│  │  • sys_pipe() crea par de FDs                          │  │
│  └────────────────────────────────────────────────────────┘  │
└──────────────────────────────────────────────────────────────┘
```

---

### 3.12. Criptografía (`kernel/crypto/`)

```
kernel/crypto/
├── sha256.c       ← SHA-256 hash (freestanding)
└── ed25519.c      ← Ed25519 firma digital (freestanding)
```

---

### 3.13. Espacio de Usuario (`userspace/`)

```
userspace/
├── Makefile           ← Build de programas userspace
├── linker.ld          ← Script de enlace para userspace
│
├── sh.c               ← Shell de usuario (simple)
├── test.c             ← Programa de prueba
├── exec_test.c        ← Test de execve()
├── test_libc_advanced.c
├── test_net.c         ← Test de networking
├── test_security.c    ← Test de seguridad
│
└── libc/              ← Biblioteca C mínima
    ├── include/       ← Headers POSIX-like
    │   ├── stdio.h · stdlib.h · string.h · unistd.h
    │   ├── errno.h · fcntl.h · ctype.h · signal.h
    │   ├── sys/stat.h · sys/types.h · sys/wait.h
    │   └── etc.
    └── src/           ← Implementaciones
        ├── crt0.asm       ← C runtime startup (call main, sys_exit)
        ├── string.c       ← strlen, strlcpy, memcpy, etc.
        ├── stdio.c        ← printf, puts, fprintf, etc.
        ├── stdlib.c       ← malloc/free (vía brk syscall)
        ├── unistd.c       ← write, read, fork, exec, waitpid, etc.
        ├── signal.c       ← signal handling (stub)
        ├── errno.c        ← errno global
        ├── ctype.c        ← isalpha, isdigit, etc.
        ├── fcntl.c        ← open flags
        └── sys/stat.c     ← stat structures
```

---

### 3.14. GFX (Gráficos) (`kernel/gfx/`)

```
kernel/gfx/
└── gfx.c              ← Primitivas gráficas básicas
                         • Requiere framebuffer activo
                         • Funciones de dibujo de bajo nivel
```

---

## 4. Flujo de Inicialización del Kernel

```
╔═══════════════════════════════════════════════════════════════════╗
║                    SECUENCIA DE BOOT COMPLETA                     ║
╚═══════════════════════════════════════════════════════════════════╝

BIOS POST → MBR (boot.asm)
    │
    ├── 1. Detectar RAM (INT 15h, E820)
    ├── 2. Habilitar A20 Gate
    ├── 3. Configurar GDT temporal
    ├── 4. Detectar framebuffer (VBE/VESA)
    ├── 5. Cargar initrd a memoria
    ├── 6. Transición a Protected Mode (32-bit)
    ├── 7. Configurar paginación (Identity Map 4GB)
    ├── 8. Transición a Long Mode (64-bit)
    └── 9. Saltar a kmain()

kmain() [kernel/main.c]
    │
    ├── hal_init()                          [HAL Layer]
    │   ├── VGA + Serial init
    │   ├── GDT + TSS
    │   ├── PIC remap
    │   ├── IDT (256 vectores)
    │   ├── SYSCALL MSRs
    │   ├── Input (Keyboard)
    │   ├── Timer (PIT 100Hz)
    │   └── STI (habilitar interrupts)
    │
    ├── acpi_init()                         [ACPI Discovery]
    │   └── Parse MADT → Detect CPUs
    │
    ├── cpu_init_bsp()                      [SMP - BSP Setup]
    │   └── Set GS_BASE per-CPU
    │
    ├── pat_init()                          [PAT config]
    │
    ├── pmm_init()                          [Physical Memory]
    │   └── Parse E820 → Build bitmap
    │
    ├── hal_mmu_init() → vmm_init()        [Virtual Memory]
    │
    ├── mm_init()                           [Heap Allocator]
    │   └── Setup heap region
    │
    ├── bcache_init()                       [Block Cache]
    │
    ├── terminal_switch_to_framebuffer()    [Switch VGA→FB]
    │
    ├── lapic_init() + smp_init()           [SMP - Boot APs]
    │   └── Send INIT+STARTUP IPI to each AP
    │
    ├── VFS Initialization                  [Filesystems]
    │   ├── initialise_initrd()  → mount at /
    │   ├── devfs_init()         → mount at /dev
    │   ├── procfs_init()        → mount at /proc
    │   └── jfs_init()           → mount at /data
    │
    ├── e1000_init() + init_network()       [Networking]
    │   ├── PCI scan → find E1000
    │   ├── lwIP init + DHCP
    │   └── task_create("Network", ...)
    │
    ├── kernel_print_banner()               [Banner]
    │
    ├── mouse_init()                        [PS/2 Mouse]
    │
    ├── scheduler_init()                    [Scheduler]
    │   └── Crea idle task (PID 0)
    │
    ├── futex_init()                        [Futexes]
    │
    ├── task_create("UserLoader", ...)      [Userspace Init]
    │   └── Carga sh.elf → Ring 3
    │
    └── shell_run()                         [Kernel Shell]
        └── Loop infinito de input
```

---

## 5. Diagrama de Interacción de Syscalls

```
┌─────────────────────────────────────────────────────────────────┐
│                  FLUJO DE UNA SYSCALL                           │
│                                                                 │
│  Userspace: syscall(SYS_write, fd, buf, count)                  │
│             mov rax, 1   ; SYS_write                            │
│             mov rdi, fd                                         │
│             mov rsi, buf                                        │
│             mov rdx, count                                      │
│             syscall        ; ← CPU trap                         │
│                                                                 │
│  ┌──────────────── Ring 0 ──────────────────────────────┐       │
│  │  syscall_entry.asm:                                  │       │
│  │    1. swapgs            ; Cambiar a kernel GS        │       │
│  │    2. Save user RSP, load kernel RSP (from TSS)      │       │
│  │    3. Push registers (rdi, rsi, rdx, r10, r8, r9)    │       │
│  │    4. call syscall_handler                           │       │
│  │                                                      │       │
│  │  syscall_handler() (syscall.c):                      │       │
│  │    switch(rax) {                                     │       │
│  │      case SYS_write: result = sys_write(fd,buf,cnt); │       │
│  │      case SYS_read:  result = sys_read(fd,buf,cnt);  │       │
│  │      case SYS_fork:  result = task_fork(regs);       │       │
│  │      case SYS_execve: result = task_exec(path,...);  │       │
│  │      ...                                             │       │
│  │    }                                                 │       │
│  │    regs->rax = result; // Return value               │       │
│  │                                                      │       │
│  │  syscall_entry.asm (return):                         │       │
│  │    5. Pop registers                                  │       │
│  │    6. Restore user RSP                               │       │
│  │    7. swapgs                                         │       │
│  │    8. sysretq          ; ← Return to Ring 3          │       │
│  └──────────────────────────────────────────────────────┘       │
│                                                                 │
│  Userspace: rax = bytes_written (resultado)                     │
└─────────────────────────────────────────────────────────────────┘
```

---

## 6. Tabla de Syscalls Implementadas

| # | Syscall | Función | Estado |
|---|---------|---------|--------|
| 0 | read | sys_read() | ✅ |
| 1 | write | sys_write() | ✅ |
| 2 | open | sys_open() | ✅ |
| 3 | close | sys_close() | ✅ |
| 4 | stat | sys_stat() | ✅ |
| 5 | fstat | sys_fstat() | ✅ |
| 8 | lseek | sys_lseek() | ✅ |
| 9 | mmap | sys_mmap() | ✅ (parcial) |
| 12 | brk | sys_brk() | ✅ |
| 16 | ioctl | sys_ioctl() | ✅ |
| 19 | readv | sys_readv() | ✅ |
| 20 | writev | sys_writev() | ✅ |
| 22 | pipe | sys_pipe() | ✅ |
| 33 | dup2 | sys_dup2() | ✅ |
| 39 | getpid | sys_getpid() | ✅ |
| 41 | socket | sys_socket() | ✅ |
| 42 | connect | sys_connect() | ✅ |
| 56 | clone | (stub) | ⚠️ |
| 57 | fork | task_fork() | ✅ |
| 59 | execve | task_exec() | ✅ |
| 60 | exit | task_exit() | ✅ |
| 61 | wait4 | task_waitpid() | ✅ |
| 62 | kill | sys_kill() | ✅ |
| 63 | uname | sys_uname() | ✅ |
| 83 | mkdir | sys_mkdir() | ✅ |
| 87 | unlink | sys_unlink() | ✅ |
| 158 | arch_prctl | sys_arch_prctl() | ✅ |
| 202 | futex | sys_futex() | ✅ |

---

## 7. Build System

```
┌──────────────────────────────────────────────────────────────┐
│                    SISTEMA DE BUILD                          │
│                                                              │
│  Makefile (principal)    ← GNU Make, 17KB                    │
│  build.ps1              ← Script PowerShell para Windows     │
│                                                              │
│  Toolchain:                                                  │
│    x86_64-elf-gcc        (Cross compiler)                    │
│    x86_64-elf-ld         (Linker)                            │
│    nasm                  (Assembler)                         │
│    QEMU                  (Emulador)                          │
│                                                              │
│  Targets:                                                    │
│    make kernel       → kernel/kernel.elf                     │
│    make userspace    → sh.elf, test.elf                      │
│    make initrd       → initrd.img (via mkinitrd.py)          │
│    make iso          → eteros.iso (Bootable)                 │
│    make run          → QEMU con E1000, serial, etc.          │
│                                                              │
│  Scripts:                                                    │
│    scripts/create_iso.sh      ← Crea ISO con GRUB/direct     │
│    scripts/setup_toolchain.sh ← Instala cross-compiler       │
│    scripts/setup_windows.ps1  ← Setup en Windows             │
│    scripts/setup_wsl.sh       ← Setup en WSL                 │
│                                                              │
│  Tools:                                                      │
│    tools/mkinitrd.py     ← Genera imagen initrd              │
│    tools/mkuefi.py       ← Genera boot UEFI                  │
│    tools/gen_logo.py     ← Genera logo en header C           │
│    tools/font_converter.ps1  ← Convierte fonts a bitmap      │
│    tools/updater/        ← Sistema de actualización          │
└──────────────────────────────────────────────────────────────┘
```

---

## 8. Testing (`tests/`)

```
47 archivos de test + benchmarks:

Tests funcionales:                  Benchmarks:
  test_string.c (28KB!)              bench_heap_performance.c
  test_heap.c                        bench_memmove.c
  test_fat32.c                       bench_memset32.c
  test_pci.c                         bench_strchr.c
  test_crypto.c                      bench_strcmp.c
  test_dhcp.c                        bench_strncmp.c
  test_klog.c                        bench_vga_clear.c
  test_stdio.c                       bench_omni_clipping.c
  test_futex_logic.c                 bench_serial_simulation.c
  test_syscall_dispatch.c
  test_vfs_leak.c
  test_vfs_path_splitting.c
  test_vga_scroll.c
  test_nvram.c
  test_rtc.c
  test_ip_aton.c
  test_partition.c
  test_kcalloc.c

Tests de seguridad:
  test_heap_security.c
  test_elf_security.c
  test_shell_security.c
  test_stack_security.c
  test_tcp_security.c
  test_dhcp_security.c
  test_initrd_security.c
  test_initrd_overflow.c
  test_elf_read_failure.c
  test_e1000_timeout.c
  test_santitravel_refactor.c
```

---

## 9. Estadísticas del Código

| Módulo | Archivos | Líneas aprox. | Descripción |
|--------|----------|---------------|-------------|
| **kernel/arch/x86_64/** | 18 C/ASM | ~4,500 | Código específico x86_64 |
| **kernel/mm/** | 3 C | ~1,300 | PMM + VMM + Heap |
| **kernel/fs/** | 8 C | ~2,500 | VFS + 5 backends |
| **kernel/net/** | ~15 C | ~2,000+ | Networking + lwIP |
| **kernel/shell/** | 8 C | ~1,000 | Shell interactivo |
| **kernel/drivers/** | 13 C | ~2,500 | Todos los drivers |
| **kernel/apps/** | 5 C | ~1,200 | Aplicaciones internas |
| **kernel/crypto/** | 2 C | ~300 | SHA-256 + Ed25519 |
| **kernel/ (root)** | 6 C | ~2,600 | task, string, klog, etc. |
| **boot/** | 4 ASM | ~1,500 | Bootloaders |
| **userspace/** | 20+ C/ASM | ~2,000 | Libc + programas |
| **tests/** | 47 C | ~5,000 | Tests + benchmarks |
| **include/** | 49 H | ~3,000 | Headers |
| **TOTAL** | **~200 archivos** | **~25,000+** | |

---

## 10. Puntos de Mejora Identificados

### 🔴 Críticos
1. **PMM/VMM OOM**: Errores recurrentes de Out-of-Memory durante boot (conversaciones recientes)
2. **Orden de inicialización**: PMM debe estar listo antes de cualquier `kmalloc()`
3. **No hay proper page reclaim**: Una vez que se agotan los frames, no hay forma de recuperar

### 🟡 Importantes
4. **fork() CoW race conditions**: La clonación de page tables no es atómica
5. **Scheduler no es SMP-aware**: El lock global del scheduler puede causar contención
6. **FAT32 sin long filename support**: Solo 8.3
7. **No hay señales POSIX** reales (kill envía directamente, no hay signal handlers)
8. **DevFS hardcodeado**: Agregar dispositivos requiere modificar código

### 🟢 Mejoras
9. **kprintf no thread-safe**: El buffer de 1024 bytes es stack-local (OK) pero no hay lock en el output
10. **No hay swap**: Todo en RAM
11. **AP cores están ociosos**: `smp_init()` los despierta pero entran en halt loop
12. **/dev/random usa xorshift simple**: No CSPRNG real

---

## 11. Diagrama de Dependencias entre Módulos

```
                        kmain()
                          │
            ┌─────────────┼─────────────────┐
            │             │                 │
            ▼             ▼                 ▼
         HAL ◄────── Memory Mgmt ◄──── Filesystems
         │   │         │  │  │              │
         │   │     PMM─┘  │  └─VMM          │
         │   │            │                 │
         │   │          Heap ──────────► VFS ──► initrd
         │   │            │              │        devfs  
         │   │            │              │        procfs
         │   │            │              │        fat32
         │   │            │              │        jfs
         │   │            │              │        bcache
         │   └──► Drivers │              │
         │        │       │              │
         │     VGA,Serial,│           ELF Loader
         │     Keyboard,  │              │
         │     Mouse,PIT  │              │
         │     PCI,E1000  │              │
         │     RTC,Disk   │              │
         │                │              │
         │           Scheduler ◄─────────┘
         │           (task.c)
         │               │
         │          ┌────┼────┐
         │          │    │    │
         │        Shell  │  User Loader
         │          │    │    │
         │          │  Network│
         │          │    │    │
         │       Futex  Sem  Pipes
         │          │    │    │
         │          └────┼────┘
         │               │
         │          Crypto (sha256, ed25519)
         │               │  
         ▼               ▼
    Arch-specific    String/stdio
    (x86_64)         (freestanding)
```

---

*Documento generado automáticamente tras revisión completa del código fuente de éterOS.*
