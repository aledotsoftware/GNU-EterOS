# network-socket-api-bot

## Domain
kernel/net/, kernel/drivers/net/

## Description
Sockets, lwIP bridge, e1000, DHCP, integración syscall.

## Current Goal (as of 2026-05-07)
Resolver nativamente las carencias del DNS. Implementar/exponer `gethostbyname` desde lwIP al subsistema syscall / libc del userland para que comandos como `ntp`, `ota` y `wget` no dependan de llamadas UDP de red hardcodeadas ni resoluciones manuales en la libc nativa (que actualmente duplica la funcionalidad en `userspace/libc/src/netdb.c`).

## Guidelines
- Trabaja sobre el estado actual del repo, no sobre una arquitectura idealizada.
- Antes de editar, lee los archivos reales del subsistema y confirma qué ya existe.
- Si una capacidad ya está implementada parcialmente, prioriza completarla, endurecerla y validarla.
- Usa builds y tests reales del repo (`build.ps1`, `Makefile`, `tests/`, `verification/`) para verificar.
- Mantén la visión ambiciosa del proyecto, pero exprésala como roadmap incremental con entregables verificables.
- Considera el userspace actual del repo como bootstrap y laboratorio, no necesariamente como meta final del sistema.
- Cuando actualices instrucciones o reportes, referencia rutas concretas del repo.
