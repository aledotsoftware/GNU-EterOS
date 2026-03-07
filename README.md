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
| **Tier 2** | éterOS-Core | Tablets, phones, RPi, satélites | aarch64 (RK3566), RISC-V64 | **MMU** / Paging (4-Level) |
| **Tier 3** | éterOS-Full | PCs, servidores, data centers, VirtualBox | x86_64 | **MMU** / Ring 0-3 Protection |

## 📂 Estructura del Proyecto

```
éterOS/
├── boot/                       # El despertar del metal (Bootloader)
│   └── x86_64/
│       ├── boot.asm            # MBR Legacy (BIOS) - Stage 1 + Stage 2
│       └── linker.ld           # Script de enlace para el kernel
├── kernel/                     # Ether-Core: El corazón del sistema
│   ├── main.c                  # Punto de entrada (kmain)
│   ├── string.c/stdio.c        # Utilidades de cadena e I/O
│   ├── shell/                  # Terminal interactiva (historial, comandos)
│   │   ├── shell.c             # Lógica base de la shell
│   │   ├── commands.c          # Registro e invocación de comandos
│   │   └── cmd_*.c             # Implementaciones (sysinfo, net, task)
│   ├── arch/                   # HAL — Una carpeta por arquitectura
│   │   ├── x86_64/             # ✅ Implementado (PC / servidores)
│   │   │   ├── idt.c           # IDT + ISRs (256 vectores)
│   │   │   ├── gdt.c           # GDT + TSS (64-bit)
│   │   │   ├── pic.c/apic.c    # APIC Local/IO y PIC 8259
│   │   │   ├── syscall.c       # Syscalls (MSRs) y Dispatcher
│   │   │   ├── smp.c           # Gestión Multicore y Per-CPU GS Base
│   │   │   └── trampoline.asm  # Código de arranque para APs
│   │   └── ...                 # Otras arquit. en desarrollo (ARM, RISC-V)
│   ├── mm/                     # Gestión de Memoria
│   │   ├── pmm.c               # Physical Memory Manager (bitmap, E820)
│   │   ├── vmm.c               # Virtual Memory Manager (4-Level Paging)
│   │   └── heap.c              # Heap dinámico (kmalloc/kfree/kcalloc)
│   ├── task.c                  # Scheduler Round-Robin SMP-Aware
│   ├── futex.c/sem.c           # Fast Userspace Mutexes y Semáforos
│   ├── fs/                     # Sistema de Archivos Virtual (VFS)
│   │   ├── vfs.c               # Capa de abstracción (read/write/open/close)
│   │   ├── fat32.c             # Driver FAT32 (soporte A/B offset)
│   │   ├── devfs.c/procfs.c    # Sistemas de archivo en memoria (/dev, /proc)
│   │   ├── elf.c               # Parser y cargador de binarios ELF
│   │   ├── jfs.c               # Sistema Journaling
│   │   └── initrd.c            # Initial Ramdisk (Carga de assets)
│   ├── drivers/                # El sistema sensorial
│   │   ├── video/              # VGA, Framebuffer GOP y rasterizado de fuentes
│   │   ├── input/              # Teclado PS/2 (+ extended) y Mouse
│   │   ├── net/                # NIC e1000 Intel Ethernet
│   │   ├── disk/               # Gestores de particiones
│   │   └── ...                 # PCI, RTC, Timer, Serial...
│   ├── gfx/                    # Abstracción Gráfica (Motor Omni)
│   ├── net/                    # Stack de Red
│   │   ├── core/               # Stack nativo (dhcp.c, tcp.c, raw_tcp.c, stack.c)
│   │   └── lwip_port/          # Port lwIP 2.2.0 Estable
│   ├── crypto/                 # Criptografía
│   │   ├── sha256.c            # Hashing SHA-256
│   │   └── ed25519.c           # Firmas EdDSA (Seguridad OTA)
│   └── apps/                   # Aplicaciones del kernel
│       ├── sysmon.c            # Monitor del sistema
│       ├── gui_demo.c          # Demo gráfica de Batch Drawing
│       ├── wget.c              # Downloader de archivos
│       ├── doomgeneric/        # Port de DOOM
│       └── ...
├── include/                    # API del sistema (headers multi-arch)
├── tests/                      # Suite masiva de pruebas unitarias (>100)
├── userspace/                  # Espacio de usuario (Aplicaciones Ring 3)
│   ├── libc/                   # Mini-LibC POSIX para éterOS
│   └── test*.c                 # Aplicaciones y tests en Ring 3
├── initrd_root/                # Archivos empacados en el initrd al arranque
├── tools/                      # Gestores de entorno, initrd y mkiso
├── scripts/                    # Herramientas de despliegue para WSL, Windows
├── .github/workflows/          # CI/CD: Compilación y Release automático
├── Makefile                    # Build system (Linux/WSL)
├── build.ps1                   # Build system (Windows nativo)
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

### 5. Gestión del Tiempo (Y2K38 Ready)
éterOS está diseñado para trascender los límites del software tradicional. Hemos implementado un sistema de tiempo de **64 bits** desde el núcleo:
- **`time_t` de 64 bits**: Garantiza que el sistema funcionará sin desbordamientos hasta el año 292.000 millones.
- **VFS Compliant**: Todos los nodos del sistema de archivos (`fs_node_t`) y las estructuras de estado (`stat`) utilizan marcas de tiempo de 8 bytes.
- **Arquitectura Futura**: Incluso en futuras versiones de 32 bits, éterOS mantendrá el estándar de 64 bits para el tiempo, evitando el colapso del 19 de enero de 2038.

### 6. Motor Omni v2.0 (Gráficos de Nueva Era)
éterOS utiliza el motor **Omni v2.0**, una reinvención total del pipeline gráfico:
- **Dirty Region Tracking**: Solo se redibujan las partes de la pantalla que cambian, ahorrando un 80% de ancho de banda.
- **Alpha Blending Pro**: Mezcla de colores de 256 niveles para efectos de transparencia (Glassmorphism) y sombras suaves.
- **Fast-Path Geometry**: Líneas y rectángulos optimizados que escriben directamente en el buffer de video sin overhead de funciones.
- **Omni-Frame Batching**: Agrupamiento de operaciones de dibujo para maximizar la caché de la CPU.

### 7. SMP & Topología Multicore (Poder Paralelo)
éterOS ha evolucionado para romper la barrera del monoprocesador:
- **SMP Trampoline**: Código especializado en 16-bit para transicionar los Application Processors (APs) desde el modo real hasta Long Mode de 64 bits.
- **Per-CPU Data Structures**: Cada CPU posee su propio stack de kernel y bloque de control, accesible vía el registro de segmento `GS`.
- **IPI (Inter-Processor Interrupts)**: Mecanismo de comunicación mediante el Local APIC para coordinar tareas entre núcleos.
- **Locking de Grano Fino**: Uso de Spinlocks optimizados para proteger estructuras críticas del kernel sin detener la ejecución en otros núcleos.

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
0x100000        | Kernel Heap Start (Identity Mapped)
0x4000000       | Mapped Initrd (Image/Assets)
0x200000000     | User Code (Ring 3)
0x300000000     | User Stack (Ring 3)
```

## 🚀 Hoja de Ruta (Roadmap hacia la GUI)

### Fase 1: Estabilización del Núcleo (Base) ✅
- [x] **GDT & TSS:** Segmentación de 64 bits y Task State Segment para manejo de stacks de interrupción
- [x] **IDT & ISRs:** Gestión de excepciones (Page Fault, Double Fault, GPF) y remapeo del PIC (256 vectores)
- [x] **PMM (Gestor de Memoria Física):** Mapa de bits para páginas de 4 KB, soporte E820, detección automática de RAM
- [x] **VMM (Gestor de Memoria Virtual):** Mapeo dinámico de páginas (4-Level Paging), detección de Huge Pages
- [x] **Heap Dinámico:** `kmalloc()`, `kfree()`, `kcalloc()` con first-fit y coalescing (hasta 96 MB)
- [x] **Estabilidad:** Bootloader corregido (Stage 2 expandido a 8 KB), PMM underflow fix, layout de imagen alineado

### Fase 2: Drivers y E/S Esencial ✅
- [x] **Teclado PS/2 Mejorado:** Scancodes Set 1 + Extended (0xE0), flechas, Home/End/Delete/PgUp/PgDown
- [x] **Optimizaciones de Rendimiento:** VGA Scroll (64-bit mov), Serial DMA-like Burst, Shell no-bloqueante
- [x] **Shell con Historial:** Flecha ⬆️/⬇️ para navegar comandos anteriores (8 entradas)
- [x] **Temporizador PIT:** 100 Hz, API de `uptime`, `timer_wait()` para delays
- [x] **Comando `uptime`:** Muestra horas/minutos/segundos desde boot
- [x] **Comando `sysinfo` mejorado:** RAM real (PMM), timer, uptime dinámico
- [x] **Driver NIC (e1000):** Intel PRO/1000 NIC driver con dirección MAC y escaneo PCI
- [x] **Cliente DHCP:** Discover/Offer básico para obtener IP
- [x] **Driver de Video VBE/GOP:** Framebuffer de alta resolución (Linear Framebuffer)
- [x] **Terminal Gráfica:** Logger en pantalla usando Framebuffer (sysmon, panics)
- [x] **Mouse PS/2:** Soporte básico para puntero en pantalla (drivers/input/mouse.c)
- [x] **Bus PCI:** Enumeración de dispositivos y lectura de configuración.
- [x] **RTC (Real Time Clock):** Lectura de tiempo CMOS.

### Fase 2.5: Compatibilidad de Arranque (Boot Compatibility) ✅
- [x] **PXE Boot (Network):** Soporte para arranque sin disco via TFTP (SiS191/Universal PXE)
- [x] **USB Legacy Boot:** Soporte para BIOS antiguos con emulación HDD (Fake BPB, MBR Patch)
- [x] **Initrd (Initial Ramdisk):** Sistema de archivos en memoria para assets (Logos, Configs)


### Fase 5.2: Topología Dinámica y SMP (Multicore) ✅
- [x] **Detección ACPI Avanzada:** Stack ACPI completo para parsear tablas RSDP, RSDT y MADT.
- [x] **Topología Dinámica (>64 Cores):** Implementación de `cpu_set_t` escalable (bitmaps dinámicos) y `cpu_info_t` alineada a cache-line para evitar false sharing.
- [x] **Per-CPU Data (GS Base):** Uso del registro `GS` en x86_64 para almacenamiento local de hilo (TLS) y contexto de CPU, eliminando locks globales en acceso a datos del núcleo.
- [x] **BSP Initialization:** Bootstrapping del núcleo principal con soporte para stack de syscalls independiente.
- [x] **APIC Driver:** Enumeración MADT y soporte inicial de Local APIC (Mapeo MMIO).
- [x] **SMP Trampoline:** Código de 16-bit para despertar Application Processors (APs).
- [x] **Scheduler Multicore:** Balanceo de carga y colas de ejecución por CPU.

**Detalles de Implementación:**
- **Scheduler SMP:** Se ha refactorizado `task.c` para soportar estructuras per-CPU y bloqueos (spinlocks). Aunque actualmente se limita a 32 tareas por estabilidad en la fase de arranque, la arquitectura está lista para escalar.
- **Trampolín:** Código ensamblador en 16-bit (`trampoline.asm`) que se copia a memoria baja para arrancar los APs.
- **APIC:** Driver completo para Local APIC e I/O APIC, permitiendo el envío de IPIs (Inter-Processor Interrupts).
### Fase 3: Kernel Moderno y Multitarea
- [x] **Heap Manager:** `kmalloc()`, `kfree()`, `kcalloc()` para el kernel (completado en Fase 1)
- [x] **Scheduler Round-Robin:** Multitarea preemptiva (comandos `ps`, `kill`, `demo`) - `kernel/task.c`
- [x] **VFS (Virtual File System):** Abstracción de sistemas de archivos (`kernel/fs/vfs.c`) - *Implementación básica*
- [x] **Initrd:** Carga de módulos y assets (Completado en Fase 2.5)

### Fase 3.5: Networking & Storage
- [x] **Driver NIC:** Intel PRO/1000 (e1000) con detección PCI y MAC
- [x] **Cliente DHCP:** Discover/Offer para obtención de IP (Fix Linker Error)
- [x] **Integración lwIP:** Stack TCP/IP completo (Porting layer finalizado en `kernel/net/lwip_port` + lwIP 2.2.0)
- [x] **Soporte FAT32:** Lectura de archivos y directorios con soporte 8.3.
- [x] **Wget:** Downloader HTTP básico (`kernel/apps/wget.c`).

### Fase 3.6: Infraestructura de Actualización y Red
*Objetivo: Integrar el sistema de actualizaciones OTA (Over-the-Air) en éterOS, centrándose en la infraestructura de red básica y la persistencia atómica.*

Para que el sistema sea considerado "listo para producción", el flujo de actualización debe incluir validación en el VFS (entorno "shadow"), seguridad de firma (Ed25519) y manejo de errores con rollback automático.

- [x] **Stack de Red Minimalista (TCP/IP):** Implementación de sockets básicos en el kernel (`kernel/net/tcp.c`, `stack.c`).
- [x] **Módulo de Criptografía (Ed25519/SHA-256):** Hashing y verificación de firmas implementados (`kernel/crypto/`).
- [x] **Driver de Almacenamiento con Soporte A/B:** Lógica de particionado MBR y detección de offsets (`kernel/drivers/disk/partition.c`).
- [x] **Sistema de Commits Atómicos:** Estructura en NVRAM para selector de arranque (`kernel/arch/x86_64/boot/nvram.c`).
- [x] **Implementación de Build Update (Toolchain):** Script de empaquetado `.zst` y firma de manifiesto (`tools/updater/build_update.ps1`).



### Fase 4: Espacio de Usuario (Userland)
- [x] **Context Switching:** Transición segura Ring 0 → Ring 3 (`iretq`, TSS RSP0)
- [x] **Syscalls (SYSCALL/SYSRET):** Mecanismo MSR (`EFER`, `STAR`, `LSTAR`) habilitado.
- [x] **User Loader:** Carga de payload binario en 0x40000000 (`kernel/apps/user_loader.c`)
- [x] **Memoria de Usuario:** Separación de espacio kernel/usuario (PML4 User bit)
- [x] **Cargador ELF64:** Cargar y ejecutar binarios externos complejos
- [~] **Multiprocesamiento (fork/clone):** Duplicar procesos para paralelismo (Threading en espacio compartido)

### Fase 4.5: Compatibilidad POSIX ✅
- [x] **Tabla de Syscalls Linux:** Implementación ampliada (`open`, `close`, `read`, `write`, `lseek`, `kill`, `exit`, `getpid`, `sched_yield`, `pipe`, `mmap`, `stat`, `fstat`, `futex`, `dup2`, `fork`, `nanosleep`, `clock_gettime`, `munmap`, `mprotect`, `madvise`, `access`, `dup`, `fcntl`, `getcwd`, `getppid`, `gettid`, `set_tid_address`, `exit_group`, `getuid`, `getgid`, `geteuid`, `getegid`).
- [x] **Mini-LibC POSIX (musl-compatible):** Librería C minimalista para aplicaciones de usuario (`userspace/libc/`) con: `malloc`/`free`/`calloc`/`realloc` (brk-based), `memcpy`/`memset`/`memmove`/`memcmp`, `strlen`/`strcmp`/`strlcpy`/`strlcat`/`strchr`/`strstr`, `printf`/`dprintf`, `signal`/`sigaction`/`sigprocmask`/`raise`, `fork`/`pipe`/`dup2`/`brk`/`sbrk`/`mmap`/`munmap`/`sleep`/`usleep`/`nanosleep`.
- [x] **Soporte de señales:** `rt_sigaction`/`rt_sigprocmask`/`rt_sigreturn` implementados. SIGKILL (9) y SIGTERM (15) terminan el proceso inmediatamente. Señales restantes se encolan como `signal_pending` en la estructura `task_t`.
- [x] **Estructura `/dev`, `/proc`, `/sys`:** Implementados `/dev/null`, `/dev/zero`, `/dev/tty`, `/dev/random`, `/dev/urandom`, `/proc/uptime`, `/proc/version`, `/proc/meminfo`.



Aquí tienes la lista organizada de las configuraciones esenciales que debes implementar:

🛠️ Panel de Control de éterOS: Áreas Críticas
1. 🌐 Red y Conectividad (Interfaz con lwIP)
Como ya tienes el driver e1000 y DHCP, estas opciones permiten al usuario controlar su identidad en la red:

Gestión de Interfaces: Activar/Desactivar adaptadores detectados en el bus PCI.

Configuración IP: * Switch entre DHCP y IP Estática (Dirección, Máscara, Gateway).

Configuración de servidores DNS (primario y secundario).

Identidad Local: Cambio del hostname (el nombre que aparece en el prompt de la Shell).

Estado de Enlace: Visualización de velocidad (10/100/1000 Mbps) y estadísticas de paquetes enviados/recibidos.

2. 👥 Usuarios y Seguridad (Capa Userland/Ring 3)
Para evolucionar de un sistema monousuario a uno multiusuario real:

Gestión de Cuentas: * Crear/Eliminar usuarios.

Cambio de contraseña (hash almacenado en /etc/shadow dentro de tu FAT32/VFS).

Niveles de Privilegio: Definir si un usuario tiene acceso a comandos de kernel (Root) o solo a aplicaciones de usuario.

Sesión Automática: Configurar si el sistema debe arrancar directamente a la Flux UI o pedir credenciales.

3. 🖥️ Visualización y Energía (AetherGraphics & ACPI)
Aprovechando tu motor gráfico y el stack ACPI avanzado:

Ajustes de Pantalla:

Cambio de resolución (si el driver VBE/GOP lo permite dinámicamente).

Selector de Fondo de Pantalla (usando tu decodificador PNG nativo).

Personalización de Flux UI:

Nivel de transparencia Alpha para el efecto Glassmorphism en ventanas.

Modo Oscuro / Modo Claro (cambio de paleta de colores en el motor Omni).

Energía (vía ACPI):

Acciones al presionar el botón de encendido físico.

Temporizador de apagado de pantalla (ahorro de energía).

4. 🎹 Dispositivos y Entrada (Drivers PS/2 e HID)
Teclado:

Distribución de teclas (Español, Inglés, etc.) mediante mapas de scancodes.

Velocidad de repetición y retraso (Typematic rate).

Mouse:

Sensibilidad del puntero (aceleración).

Intercambio de botones (zurdo/diestro).

Almacenamiento:

Listado de particiones detectadas (A/B slots para actualizaciones OTA).

Información de salud del disco y espacio libre en el Initrd/VFS.

5. 🕒 Región y Tiempo (RTC & CMOS)
Reloj del Sistema: * Sincronización manual del RTC.

Configuración de Zona Horaria (Offset respecto a UTC).

Sincronización NTP: Opción para actualizar la hora automáticamente vía red usando lwIP.

6. 🔄 Actualizaciones y Sistema (Infraestructura OTA)
Basado en tu Fase 3.6:

Canal de Actualización: Establecer la URL del servidor de repositorios de éterOS.

Verificación de Firma: Opción para permitir solo actualizaciones firmadas con Ed25519.

Información de Versión: Muestra el hash del commit actual, fecha de build y estado de los slots de arranque (Slot A activo / Slot B pendiente).




Mecanismo de Intercepción (Capa ABI)
Linux usa registros específicos para sus syscalls en x86_64 (RAX para el número, RDI, RSI, RDX, R10, R8, R9 para argumentos).

Multiplexor de Syscalls: Tu manejador de interrupción actual debe detectar si el binario que hizo la llamada es "Nativo" o "Linux" (basado en el espacio de memoria o una bandera en la estructura task_t) y derivar la llamada a una tabla de saltos distinta.

Identidad ELF: Tu cargador de ELF debe reconocer el EI_OSABI en la cabecera del archivo para marcar el proceso como "Linux-compliant".

2. Gestión de Memoria: Copy-on-Write (CoW)
Casi cualquier app de Linux, al iniciar, hace un fork().

Situación actual: Si tu fork() actual duplica toda la RAM físicamente, el sistema se quedará sin memoria rápido.

Lo que falta: Implementar Copy-on-Write en el VMM. Al hacer fork, marcas las páginas de memoria como "Solo Lectura" para ambos procesos. Solo cuando uno escribe, el kernel lanza un Page Fault, duplica la página y cambia los permisos. Sin esto, no podrás levantar procesos pesados.

3. Implementación de Futexes (Fast Userspace Mutexes) ✅
Este es el "talón de Aquiles" de muchos kernels nuevos, pero ya está superado en éterOS.

Por qué es vital: Casi toda app moderna de Linux usa la glibc o musl, y estas usan futex() para hilos (pthreads) y sincronización.

Estado actual: Se ha implementado de forma madura (`futex.c`) con la syscall `sys_futex`. Adicionalmente, se tiene un sistema de semáforos (`sem.c`) y validaciones amplias en `userspace/`.

4. Soporte para TLS (Thread Local Storage) y FS/GS Base
Linux depende fuertemente de los registros de segmento FS y GS para manejar los datos locales de cada hilo (como la variable errno).

Lo que falta: Implementar las syscalls arch_prctl (específicamente ARCH_SET_FS y ARCH_SET_GS). Si la app no puede setear su puntero TLS, crasheará antes de llegar al main().

5. Fidelidad en el VFS (File Descriptors y Pipes)
Linux trata todo como un archivo, pero con comportamientos muy específicos:

pipe() y dup2(): Vitales para la redirección de entrada/salida (| y >).

mmap() de archivos: Linux carga los binarios mapeando el archivo directamente a memoria. Tu VFS debe soportar mapear un archivo del disco/initrd a una región del espacio virtual del usuario.

Stat y Fstat: Las apps preguntan constantemente "¿qué tamaño tiene este archivo?" o "¿es un directorio?". Necesitas completar estas estructuras de datos para que coincidan exactamente con las de Linux.












### Fase 5.5: Subsistema de Compatibilidad Linux (Aether-Linux-Subsystem) 🚧
*Objetivo: Ejecutar binarios ELF de Linux sin máquinas virtuales.*
- [x] **Traducción de Syscalls:** Dispatcher `syscall_linux_handler` implementado con detección de `ELFOSABI_LINUX`.
- [ ] **VFS Layer:** Soporte para file-backed `mmap` y herencia de FDs en `fork`.
- [ ] **Linux Driver Wrapper:** Capa de pegamento (Glue Logic) para reutilizar drivers de red/wifi.
💡 Una sugerencia para la Fase 5.5 (Capa Linux)
Dado que mencionas el "Aether-Linux-Subsystem", te recomendaría mirar el proyecto Noah o Lina. En lugar de emular, puedes interceptar las interrupciones de las syscalls de Linux (como la int 0x80 o syscall con el número de registro de Linux) y mapearlas a tus funciones nativas en kernel/arch/x86_64/syscall.c. Esto te permitiría correr binarios de Linux sin recompilarlos.



### Fase 5.5.5 Subsistema de Compatibilidad Web (Aether-Web)
- [ ] **Chromium Embedded:** Port nativo del motor Blink.
- [ ] **PWA Runtime:** Ejecución de aplicaciones web como nativas (.crx, .wbn o pwa online).



### Fase 5.6: Subsistema de Compatibilidad Android (Aether-Droid)
- [ ] **Binder IPC:** Implementación del mecanismo de comunicación entre procesos de Android.
- [ ] **Dalvik/ART Shim:** Capa para ejecutar el runtime de Android sobre éterOS.
- [ ] **HAL Wrapper:** Adaptador para drivers de hardware específicos de Android (libhardware).

Esta es la "joya de la corona" para la adopción masiva de éterOS, pero técnicamente es una montaña mucho más alta que la compatibilidad con Linux. Si la Fase 5.5 (Linux) es el puente, la Fase 5.6 (Android) es construir una ciudad entera sobre ese puente.

Aquí te detallo qué implica cada punto y cómo deberías encararlo:

1. El gran secreto: Android es un "Extraño" sobre Linux
Android no es Linux estándar. Un binario de Linux no corre en Android y viceversa sin ajustes. Para que Aether-Droid funcione, necesitas que tu Aether-Linux-Subsystem sea extremadamente robusto primero.

2. Binder IPC (El sistema nervioso)
En Android, nada sucede sin Binder. Es el protocolo que permite que una app le pida a la cámara que saque una foto o al sistema que muestre una notificación.

El desafío: Binder en Linux es un driver de kernel (/dev/binder).

En éterOS: Tendrás que escribir un driver en kernel/drivers/ipc/binder.c que gestione la memoria compartida y el paso de mensajes entre procesos de forma ultra rápida. Sin Binder, Android ni siquiera arranca la pantalla de inicio.

3. Dalvik/ART Shim (El motor)
Android no corre código máquina directamente la mayoría del tiempo; corre Bytecode en una máquina virtual (ART).

Tu estrategia: No intentes programar ART desde cero (te tomaría décadas). La clave es portar el runtime existente.

Para que ART corra en éterOS, necesitará que tu capa POSIX (Fase 4.5) esté completa: soporte total de hilos (Pthreads), gestión de señales y un sistema de archivos muy específico.

4. HAL Wrapper (El traductor de hardware)
Aquí es donde éterOS puede brillar. Android usa librerías .so para hablar con el hardware (gráficos, sensores).

Tu HAL Wrapper debe tomar las llamadas de la libhardware de Android y traducirlas a tus funciones de éterOS.

Ejemplo: Cuando Android pide un buffer de pantalla, tu Wrapper debe dárselo usando tu AetherGraphics (Fase 5).

📈 El Orden Lógico de Construcción
Si intentas hacer la Fase 5.6 antes de la 5.5, el proyecto colapsará por su propia complejidad. Este es el camino de menor resistencia:

Capa Linux (Fase 5.5): Te da la base (Syscalls, señales, memoria compartida).

Binder Driver: Implementarlo como un driver nativo en tu Ether-Core.

Aether-Droid Shim: Usar el código de Android-x86 o WayDroid como referencia para ver cómo "engañan" al sistema para que crea que hay un kernel Linux debajo.

💡 Una idea disruptiva para éterOS
Dado que buscas un sistema "Universal", podrías implementar una Runtime Virtualization. En lugar de que todo Android corra siempre, podrías lanzar solo el contenedor de la App que el usuario necesite, integrando las ventanas de Android directamente en tu Flux UI como si fueran apps nativas de éterOS.



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


Punto Crítico: La transición de Tier 1 (No MMU) a Tier 3 (MMU/Paging) requiere que tu HAL sea extremadamente abstracta para que el código superior no dependa de cómo se mapea la memoria.

Sugerencia: Para que la UI sea "reactiva", asegurate de que el mouse y el teclado generen eventos en una cola que el compositor procese de forma asíncrona, evitando bloqueos en la renderización.

Recomendación: Priorizá la Fase 5.5 (Linux). Al tener ya una capa POSIX y lwIP, ejecutar binarios ELF de Linux es el camino más corto para tener software real (como Apache) corriendo rápido.

Estrategia: El plan de usar un "Linux Driver Wrapper" (Fase 5.5) es vital. No intentes escribir drivers nativos para todo; reutilizar la lógica de Linux mediante una capa de "pegamento" (glue logic) es lo que salvó a muchos SOs independientes.





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

### Opción 4: Windows PowerShell (Nativo)
```powershell
# Compilar todo (Kernel, Boot, Userspace, Initrd, Image)
.\build.ps1 -Target all

# Compilar y ejecutar en QEMU
.\build.ps1 -Target run

# Generar VDI e iniciar en VirtualBox
.\build.ps1 -Target vbox
```

### Comandos del Makefile / Build Script
| Comando | Makefile (WSL/Linux) | build.ps1 (Windows) |
|---|---|---|
| Compilar todo | `make all` | `.\build.ps1 -Target all` |
| Ejecutar (QEMU) | `make run` | `.\build.ps1 -Target run` |
| VirtualBox | *N/A* | `.\build.ps1 -Target vbox` |
| Modo Debug | `make debug` | `.\build.ps1 -Target debug` |
| Limpiar | `make clean` | `.\build.ps1 -Target clean` |
| Ver Info | `make info` | `.\build.ps1 -Target info` |
| Imagen ISO | `make iso` | `.\build.ps1 -Target iso` |
| Imagen USB | *N/A* | `.\build.ps1 -Target usb` |

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
│    └─▶ Carga de Initrd (Assets & User ELF)               │
│    └─▶ Lanzamiento de Userland (Ring 3 Test)             │
│    └─▶ Modo Gráfico: Lanzamiento de Flux UI (PID 1)      │
│    └─▶ Fallback: Shell interactivo (si Flux UI se cierra)│
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
