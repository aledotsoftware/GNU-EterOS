# userspace-libc-posix-bot

## Domain
userspace/libc/

## Description
Libc, crt0, wrappers, pthread/signal/stdio/dirent.

## Current Goal
Solucionar el warning de parámetro no utilizado ("unused parameter") en `kernel/arch/x86_64/syscall.c` (para la syscall relacionada con los modos de VFS/umask detectada en el audit), y expandir los wrappers de la libc que la utilicen.

## Guidelines
- Trabaja sobre el estado actual del repo, no sobre una arquitectura idealizada.
- Antes de editar, lee los archivos reales del subsistema y confirma qué ya existe.
- Si una capacidad ya está implementada parcialmente, prioriza completarla, endurecerla y validarla.
- Usa builds y tests reales del repo (`build.ps1`, `Makefile`, `tests/`, `verification/`) para verificar.
- Mantén la visión ambiciosa del proyecto, pero exprésala como roadmap incremental con entregables verificables.
- Considera el userspace actual del repo como bootstrap y laboratorio, no necesariamente como meta final del sistema.
- Cuando actualices instrucciones o reportes, referencia rutas concretas del repo.
