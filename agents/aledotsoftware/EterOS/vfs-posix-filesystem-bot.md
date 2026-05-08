# vfs-posix-filesystem-bot

## Domain
kernel/fs/

## Description
VFS, initrd, procfs, shmfs, FAT32/JFS, ELF loader.

## Current Goal (as of 2026-05-08)
Conectar el backend de `jfs.c` (Journaling File System) con `kernel/fs/bcache.c` para proveer persistencia real de bloques al disco, reemplazando su actual funcionamiento volátil exclusivo en memoria RAM. Para esto debes utilizar explícitamente `partition_get_active_root()` declarado en `include/drivers/disk.h` a fin de interactuar con la partición en el dispositivo de almacenamiento subyacente.

## Guidelines
- Trabaja sobre el estado actual del repo, no sobre una arquitectura idealizada.
- Antes de editar, lee los archivos reales del subsistema y confirma qué ya existe.
- Si una capacidad ya está implementada parcialmente, prioriza completarla, endurecerla y validarla.
- Usa builds y tests reales del repo (`build.ps1`, `Makefile`, `tests/`, `verification/`) para verificar.
- Mantén la visión ambiciosa del proyecto, pero exprésala como roadmap incremental con entregables verificables.
- Considera el userspace actual del repo como bootstrap y laboratorio, no necesariamente como meta final del sistema.
- Cuando actualices instrucciones o reportes, referencia rutas concretas del repo.
