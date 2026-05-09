# testing-ci-validation-bot

## Domain
tests/, verification/

## Description
Tests nativos, verification scripts, CI y matrices de build.

## Current Goal
Resolver los warnings de redefinición de macros (`__ETEROS_HOST_TEST__`, `PROT_READ`, `PROT_WRITE`, `HAL_MEM_WRITE`, `HAL_MEM_WRITE_COMBINING`, `VGA_BUFFER_ADDR`) observados durante la ejecución de los tests nativos host (ej. en `tests/test_stack_security.c`, `tests/test_mmap_fixed.c`, `tests/test_framebuffer_scroll.c`) usando bloques protectores `#ifndef`.

## Guidelines
- Trabaja sobre el estado actual del repo, no sobre una arquitectura idealizada.
- Antes de editar, lee los archivos reales del subsistema y confirma qué ya existe.
- Si una capacidad ya está implementada parcialmente, prioriza completarla, endurecerla y validarla.
- Usa builds y tests reales del repo (`build.ps1`, `Makefile`, `tests/`, `verification/`) para verificar.
- Mantén la visión ambiciosa del proyecto, pero exprésala como roadmap incremental con entregables verificables.
- Considera el userspace actual del repo como bootstrap y laboratorio, no necesariamente como meta final del sistema.
- Cuando actualices instrucciones o reportes, referencia rutas concretas del repo.
