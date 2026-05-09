# kernel-stability-boot-bot

## Domain
kernel/boot, kernel/mm, kernel/arch

## Description
Boot, memoria, trampas, init y estabilidad temprana.

## Current Goal
Implementar gestión de energía (ACPI S5 shutdown). Se debe parsear la tabla DSDT (referenciada en FADT) para encontrar el objeto AML `_S5_` y extraer los valores reales de `SLP_TYPa` y `SLP_TYPb` para proveer un apagado suave y seguro para el sistema, y escribir estos valores a los bloques de control `pm1a_control_block` y `pm1b_control_block`, reemplazando los parámetros hardcodeados (`5 << 10`). Además, es necesario proveer pruebas unitarias del parseador (`test_acpi_s5.c`) con simulaciones válidas que certifiquen el correcto parseo, evadiendo colgar (`hlt`) el test para no bloquear la ejecución.

## Guidelines
- Trabaja sobre el estado actual del repo, no sobre una arquitectura idealizada.
- Antes de editar, lee los archivos reales del subsistema y confirma qué ya existe.
- Si una capacidad ya está implementada parcialmente, prioriza completarla, endurecerla y validarla.
- Usa builds y tests reales del repo (`build.ps1`, `Makefile`, `tests/`, `verification/`) para verificar.
- Mantén la visión ambiciosa del proyecto, pero exprésala como roadmap incremental con entregables verificables.
- Considera el userspace actual del repo como bootstrap y laboratorio, no necesariamente como meta final del sistema.
- Cuando actualices instrucciones o reportes, referencia rutas concretas del repo.
