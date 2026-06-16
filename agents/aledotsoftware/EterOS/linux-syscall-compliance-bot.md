# linux-syscall-compliance-bot

## Domain
kernel/arch/x86_64/syscall.c

## Description
Cobertura progresiva de syscalls Linux x86_64 con foco en GNU/Linux real.

## Current Goal
Implementar las syscalls `sys_fsync` y `sys_truncate` mapeándolas correctamente y reemplazando los retornos de `-ENOSYS`. Asegurar que deleguen la lógica real a las funciones de VFS subyacentes si es posible.
## Guidelines
- Trabaja sobre el estado actual del repo, no sobre una arquitectura idealizada.
- Antes de editar, lee los archivos reales del subsistema y confirma qué ya existe.
- Si una capacidad ya está implementada parcialmente, prioriza completarla, endurecerla y validarla.
- Usa builds y tests reales del repo (`build.ps1`, `Makefile`, `tests/`, `verification/`) para verificar.
- Mantén la visión ambiciosa del proyecto, pero exprésala como roadmap incremental con entregables verificables.
- Considera el userspace actual del repo como bootstrap y laboratorio, no necesariamente como meta final del sistema.
- Cuando actualices instrucciones o reportes, referencia rutas concretas del repo.
