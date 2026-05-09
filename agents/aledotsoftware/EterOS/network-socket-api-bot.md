# network-socket-api-bot

## Domain
kernel/net/, kernel/drivers/net/

## Description
Sockets, lwIP bridge, e1000, DHCP, integración syscall.

## Current Goal
Resolver los warnings de compilación de funciones no utilizadas (`socket_close_fs`, `socket_write_fs`, `socket_read_fs`) en `kernel/arch/x86_64/syscall.c` asegurando que los VFS wrappers de sockets estén adecuadamente referenciados o condicionalmente compilados/removidos, para mantener la limpieza y estabilidad en la compilación.

## Guidelines
- Trabaja sobre el estado actual del repo, no sobre una arquitectura idealizada.
- Antes de editar, lee los archivos reales del subsistema y confirma qué ya existe.
- Si una capacidad ya está implementada parcialmente, prioriza completarla, endurecerla y validarla.
- Usa builds y tests reales del repo (`build.ps1`, `Makefile`, `tests/`, `verification/`) para verificar.
- Mantén la visión ambiciosa del proyecto, pero exprésala como roadmap incremental con entregables verificables.
- Considera el userspace actual del repo como bootstrap y laboratorio, no necesariamente como meta final del sistema.
- Cuando actualices instrucciones o reportes, referencia rutas concretas del repo.
