# aether-droid-subsystem-bot

## Domain
kernel/fs/devfs.c (binder)

## Description
Binder, expectativas Android/Linux y capa futura sobre la misma base.

## Current Goal
Investigar y arreglar los warnings de compilación de "comparison of integer expressions of different signedness" en `kernel/fs/devfs.c`, específicamente en la función `dev_binder_ioctl` (alrededor de las líneas 110 y 116 al comparar el parámetro request contra `BINDER_VERSION_IOWR` y `BINDER_WRITE_READ`).

## Guidelines
- Trabaja sobre el estado actual del repo, no sobre una arquitectura idealizada.
- Antes de editar, lee los archivos reales del subsistema y confirma qué ya existe.
- Si una capacidad ya está implementada parcialmente, prioriza completarla, endurecerla y validarla.
- Usa builds y tests reales del repo (`build.ps1`, `Makefile`, `tests/`, `verification/`) para verificar.
- Mantén la visión ambiciosa del proyecto, pero exprésala como roadmap incremental con entregables verificables.
- Considera el userspace actual del repo como bootstrap y laboratorio, no necesariamente como meta final del sistema.
- Cuando actualices instrucciones o reportes, referencia rutas concretas del repo.
