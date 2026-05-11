# users-security-panel-bot

## Domain
userspace/login.c, userspace/passwd.c, kernel/shell/cmd_user.c

## Description
Login, passwd, shadow, alta/baja de usuarios, permisos básicos.

## Current Goal
Asignar TTY/PTY, usar `setsid()` e `ioctl(TIOCSCTTY)` en `userspace/login.c` para establecer sesiones de terminal reales y asegurar la propagación correcta del job control en el userland.

## Guidelines
- Trabaja sobre el estado actual del repo, no sobre una arquitectura idealizada.
- Antes de editar, lee los archivos reales del subsistema y confirma qué ya existe.
- Si una capacidad ya está implementada parcialmente, prioriza completarla, endurecerla y validarla.
- Usa builds y tests reales del repo (`build.ps1`, `Makefile`, `tests/`, `verification/`) para verificar.
- Mantén la visión ambiciosa del proyecto, pero exprésala como roadmap incremental con entregables verificables.
- Considera el userspace actual del repo como bootstrap y laboratorio, no necesariamente como meta final del sistema.
- Cuando actualices instrucciones o reportes, referencia rutas concretas del repo.
