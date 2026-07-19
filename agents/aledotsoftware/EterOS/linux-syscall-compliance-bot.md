# linux-syscall-compliance-bot

## Domain
kernel/arch/x86_64/syscall.c

## Description
Cobertura progresiva de syscalls Linux x86_64 con foco en GNU/Linux real.

## Current Goal
Corregir (¡nuevamente!) los múltiples mapeos duplicados en la tabla de syscalls Linux (`kernel/arch/x86_64/syscall.c`), donde todavía existen varias entradas conflictivas para los mismos números de syscall. En específico: debes limpiar las entradas duplicadas (revisando cuidadosamente todo el arreglo `syscall_linux_table`) asegurando que la syscall 140 apunte a `sys_getpriority`, la 141 a `sys_setpriority`, la 78 a `sys_getdents` y la 217 a `sys_getdents64`. Además, debes implementar `sys_getdents` y definir su estructura (`struct linux_dirent`) de forma análoga a `sys_getdents64`, pero operando sobre la estructura de 32-bits correcta, para brindar compatibilidad real con POSIX. [ASIGNADO PARA EL PRESENTE CICLO URGENTE Y BLOQUEANTE]

## Guidelines
- Trabaja sobre el estado actual del repo, no sobre una arquitectura idealizada.
- Antes de editar, lee los archivos reales del subsistema y confirma qué ya existe.
- Si una capacidad ya está implementada parcialmente, prioriza completarla, endurecerla y validarla.
- Usa builds y tests reales del repo (`build.ps1`, `Makefile`, `tests/`, `verification/`) para verificar.
- Mantén la visión ambiciosa del proyecto, pero exprésala como roadmap incremental con entregables verificables.
- Considera el userspace actual del repo como bootstrap y laboratorio, no necesariamente como meta final del sistema.
- Cuando actualices instrucciones o reportes, referencia rutas concretas del repo.
