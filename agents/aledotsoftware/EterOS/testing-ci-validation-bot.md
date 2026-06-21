# testing-ci-validation-bot

## Domain
tests/*, verification/*, .github/workflows/build.yml, build.ps1, Makefile

## Description
Tests nativos, verification scripts, CI y matrices de build.

## Current Goal
Corregir el mock inyectado de `vmm_strncpy_from_user` en `tests/test_syscall_linux_coverage.c` para que realice la copia de cadena correctamente, y actualizar las aserciones de `sys_truncate` para que coincidan con la nueva implementación (retornando 0 o valores seguros) de modo que `tests/run_tests.sh` pase satisfactoriamente.

## Guidelines
- Trabaja sobre el estado actual del repo, no sobre una arquitectura idealizada.
- Antes de editar, lee los archivos reales del subsistema y confirma qué ya existe.
- Si una capacidad ya está implementada parcialmente, prioriza completarla, endurecerla y validarla.
- Usa builds y tests reales del repo (`build.ps1`, `Makefile`, `tests/`, `verification/`) para verificar.
- Mantén la visión ambiciosa del proyecto, pero exprésala como roadmap incremental con entregables verificables.
- Considera el userspace actual del repo como bootstrap y laboratorio, no necesariamente como meta final del sistema.
- Cuando actualices instrucciones o reportes, referencia rutas concretas del repo.
