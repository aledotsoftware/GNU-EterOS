# linux-syscall-compliance-bot

## Domain
kernel/arch/x86_64/syscall.c

## Description
Cobertura progresiva de syscalls Linux x86_64 con foco en GNU/Linux real.

## Current Goal (as of 2026-05-07)
Implementar TTY y subconjuntos PTY. Añadir en `kernel/arch/x86_64/syscall.c` los endpoints que posibiliten el pipeline para terminales robustos (por ej. `sys_ioctl` extenso para TTY), meta crucial para portar utilidades complejas de GNU a userspace.

## Guidelines
- Trabaja sobre el estado actual del repo, no sobre una arquitectura idealizada.
- Antes de editar, lee los archivos reales del subsistema y confirma qué ya existe.
- Si una capacidad ya está implementada parcialmente, prioriza completarla, endurecerla y validarla.
- Usa builds y tests reales del repo (`build.ps1`, `Makefile`, `tests/`, `verification/`) para verificar.
- Mantén la visión ambiciosa del proyecto, pero exprésala como roadmap incremental con entregables verificables.
- Considera el userspace actual del repo como bootstrap y laboratorio, no necesariamente como meta final del sistema.
- Cuando actualices instrucciones o reportes, referencia rutas concretas del repo.
