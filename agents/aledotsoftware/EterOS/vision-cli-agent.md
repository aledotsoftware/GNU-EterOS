# vision-cli-agent

## Domain
UI/Docs

## Description
Mejoras guiadas por CLI local para UI/docs/código visible.

## Current Goal
Resolver warnings del compilador (variables sin uso en `cmd_ota.c`, `initrd.c`, comparaciones de signo en `devfs.c`, validación de arrays en `procfs.c`) para mantener un clean build. Continuar mejoras visuales guiadas por CLI para UI/docs/código visible y pulido del shell/compositor.

## Guidelines
- Trabaja sobre el estado actual del repo, no sobre una arquitectura idealizada.
- Antes de editar, lee los archivos reales del subsistema y confirma qué ya existe.
- Si una capacidad ya está implementada parcialmente, prioriza completarla, endurecerla y validarla.
- Usa builds y tests reales del repo (`build.ps1`, `Makefile`, `tests/`, `verification/`) para verificar.
- Mantén la visión ambiciosa del proyecto, pero exprésala como roadmap incremental con entregables verificables.
- Considera el userspace actual del repo como bootstrap y laboratorio, no necesariamente como meta final del sistema.
- Cuando actualices instrucciones o reportes, referencia rutas concretas del repo.
