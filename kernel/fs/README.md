# éterOS - Sistema de Archivos

Este directorio contiene la implementación del Virtual File System (VFS) y todos los drivers de filesystem de éterOS.

## Propósito

El sistema de archivos de éterOS proporciona:
- Abstracción unificada para acceso a archivos y directorios
- Soporte para múltiples tipos de filesystem montados simultáneamente
- Capa de caché para optimizar acceso a disco
- Filesystems virtuales para exponer información del kernel

## Arquitectura del VFS

El VFS (Virtual File System) actúa como capa de abstracción entre las syscalls de usuario y los drivers de filesystem específicos. Proporciona una API unificada independiente del tipo de filesystem subyacente.

```
Userspace
    ↓
Syscalls (open, read, write, close)
    ↓
VFS Layer (vfs.c)
    ↓
┌─────────┬─────────┬─────────┬─────────┬─────────┬─────────┐
│ InitRD  │ FAT32   │   JFS   │ DevFS   │ ProcFS  │ ShmFS   │
└─────────┴─────────┴─────────┴─────────┴─────────┴─────────┘
    ↓         ↓         ↓
  RAM     Disk      RAM
```

## Estructura de Archivos

```
fs/
├── vfs.c       Virtual File System - Capa de abstracción
├── bcache.c    Block Cache - Caché de bloques de disco
├── elf.c       Cargador de ejecutables ELF64
├── initrd.c    Initial RAM Disk - Filesystem de boot
├── fat32.c     Driver de FAT32 (lectura/escritura)
├── jfs.c       Journaling File System (experimental)
├── devfs.c     Device File System - Nodos de dispositivos
├── procfs.c    Process File System - Info del kernel
└── shmfs.c     Shared Memory File System - Memoria compartida
```

## Drivers de Filesystem

### InitRD (initrd.c)
- **Tipo**: RAM-based, solo lectura
- **Propósito**: Filesystem de arranque cargado por el bootloader
- **Contenido**: Binarios esenciales (sh.elf, test.elf), assets gráficos
- **Punto de montaje**: `/` (root)
- **Formato**: Archivo TAR simple sin compresión

### FAT32 (fat32.c)
- **Tipo**: Disco, lectura/escritura
- **Propósito**: Interoperabilidad con Windows/Linux, almacenamiento persistente
- **Características**:
  - Soporte para nombres 8.3
  - Lectura de FAT (File Allocation Table)
  - Navegación de clusters
  - Soporte para offsets de partición MBR
- **Punto de montaje**: `/mnt/`
- **Limitaciones**: No soporta nombres largos (LFN) aún

### JFS (jfs.c)
- **Tipo**: RAM-based, experimental
- **Propósito**: Journaling File System para integridad de datos
- **Características**:
  - Journal de transacciones
  - Recuperación ante fallos
  - Actualmente solo en memoria
- **Punto de montaje**: `/data/`
- **Estado**: Prototipo, falta persistencia a disco

### DevFS (devfs.c)
- **Tipo**: Virtual, dinámico
- **Propósito**: Exponer dispositivos como archivos
- **Dispositivos disponibles**:
  - `/dev/null` - Descarta todo lo escrito
  - `/dev/zero` - Retorna bytes cero infinitos
  - `/dev/tty` - Terminal actual
  - `/dev/random`, `/dev/urandom` - Generador de números aleatorios (PRNG)
  - `/dev/input` - Ring buffer de eventos de teclado/mouse
- **Punto de montaje**: `/dev/`

### ProcFS (procfs.c)
- **Tipo**: Virtual, solo lectura
- **Propósito**: Exponer información del kernel y procesos
- **Archivos disponibles**:
  - `/proc/uptime` - Tiempo desde el boot
  - `/proc/version` - Versión del kernel
  - `/proc/meminfo` - Estadísticas de memoria
  - `/proc/cpuinfo` - Información de CPU (futuro)
- **Punto de montaje**: `/proc/`

### ShmFS (shmfs.c)
- **Tipo**: RAM-based, lectura/escritura
- **Propósito**: Memoria compartida entre procesos
- **Características**:
  - Soporte para `mmap(MAP_SHARED)`
  - Zero-copy IPC para compositor gráfico
  - Buffers compartidos entre kernel y userspace
- **Punto de montaje**: `/dev/shm/`

## Árbol de Montajes

El árbol de filesystem de éterOS en runtime:

```
/                   (InitRD - solo lectura)
├── sh.elf
├── test.elf
├── logo.png
├── dev/            (DevFS - virtual)
│   ├── null
│   ├── zero
│   ├── tty
│   ├── random
│   ├── urandom
│   ├── input
│   └── shm/        (ShmFS - memoria compartida)
├── proc/           (ProcFS - virtual)
│   ├── uptime
│   ├── version
│   └── meminfo
├── mnt/            (FAT32 - disco)
│   └── [archivos del disco]
└── data/           (JFS - experimental)
    └── [archivos journaled]
```

## Block Cache (bcache.c)

El block cache optimiza el acceso a disco mediante:
- Caché de bloques de 512 bytes
- Política LRU (Least Recently Used)
- Write-back para reducir escrituras a disco
- Sincronización periódica

## Cargador ELF (elf.c)

Parsea y carga ejecutables ELF64:
- Valida magic number y arquitectura
- Mapea segmentos `PT_LOAD` en memoria virtual
- Configura entry point y stack
- Soporta ejecutables estáticos (no dinámicos aún)

## API del VFS

### Funciones Principales

```c
// Montar un filesystem
int mount(const char *source, const char *target, const char *fstype);

// Operaciones de archivo
int open_fs(const char *path, int flags);
ssize_t read_fs(int fd, void *buf, size_t count);
ssize_t write_fs(int fd, const void *buf, size_t count);
int close_fs(int fd);

// Operaciones de directorio
int mkdir_fs(const char *path, mode_t mode);
int readdir_fs(int fd, struct dirent *entry);

// Información de archivo
int stat_fs(const char *path, struct stat *buf);
```

## Puntos de Entrada Principales

### Inicialización
1. `vfs_init()` - Inicializa el VFS y monta root
2. `initrd_init()` - Monta InitRD en `/`
3. `devfs_init()` - Monta DevFS en `/dev/`
4. `procfs_init()` - Monta ProcFS en `/proc/`
5. `fat32_mount()` - Monta partición FAT32 si se detecta

### Syscalls
Las syscalls de filesystem se mapean a funciones VFS:
- `sys_open()` → `open_fs()`
- `sys_read()` → `read_fs()`
- `sys_write()` → `write_fs()`
- `sys_close()` → `close_fs()`
- `sys_stat()` → `stat_fs()`

## Referencias

- Documentación de API VFS: `docs/api/vfs.md`
- Documentación detallada de filesystems: `docs/filesystem.md`
- Especificación ELF: `include/elf.h`
- Estructuras de VFS: `include/fs/vfs.h`
