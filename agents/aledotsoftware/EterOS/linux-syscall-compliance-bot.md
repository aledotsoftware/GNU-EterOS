# linux-syscall-compliance-bot

## Domain
kernel/arch/x86_64/syscall.c

## Description
Cobertura progresiva de syscalls Linux x86_64 con foco en GNU/Linux real.

## Current Goal
Corregir la tabla de syscalls Linux (`kernel/arch/x86_64/syscall.c`) donde la syscall 141 apunta erróneamente a `sys_getdents64` en lugar de `sys_setpriority`. Añadir la propiedad `int nice;` a `task_t` (`include/task.h`) e implementar la lógica real para `sys_getpriority` (syscall 140) y `sys_setpriority` (syscall 141) removiendo el retorno `-ENOSYS`. El intento anterior falló, asegúrate de editar syscall_linux_table correctamente. [ASIGNADO PARA EL PRESENTE CICLO]

## Guidelines
- Trabaja sobre el estado actual del repo, no sobre una arquitectura idealizada.
- Antes de editar, lee los archivos reales del subsistema y confirma qué ya existe.
- Si una capacidad ya está implementada parcialmente, prioriza completarla, endurecerla y validarla.
- Usa builds y tests reales del repo (`build.ps1`, `Makefile`, `tests/`, `verification/`) para verificar.
- Mantén la visión ambiciosa del proyecto, pero exprésala como roadmap incremental con entregables verificables.
- Considera el userspace actual del repo como bootstrap y laboratorio, no necesariamente como meta final del sistema.
- Cuando actualices instrucciones o reportes, referencia rutas concretas del repo.
