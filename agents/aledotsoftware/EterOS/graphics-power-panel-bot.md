# graphics-power-panel-bot

## Domain
kernel/gfx/, kernel/drivers/video/

## Description
Framebuffer/gfx/window/UI shell y panel gráfico.

## Current Goal
Implementar prototipo de animación del compositor visual (`test_compositor`) en `kernel/shell/cmd_misc.c` utilizando APIs del kernel como `timer_sleep` y `window_invalidate` (y `compositor_render` si es necesario), probando que las ventanas se muevan en bucle en el entorno gráfico, sentando bases prácticas para GNU Desktop sobre EterOS. [ASIGNADO PARA EL PRESENTE CICLO]

## Guidelines
- Trabaja sobre el estado actual del repo, no sobre una arquitectura idealizada.
- Antes de editar, lee los archivos reales del subsistema y confirma qué ya existe.
- Si una capacidad ya está implementada parcialmente, prioriza completarla, endurecerla y validarla.
- Usa builds y tests reales del repo (`build.ps1`, `Makefile`, `tests/`, `verification/`) para verificar.
- Mantén la visión ambiciosa del proyecto, pero exprésala como roadmap incremental con entregables verificables.
- Considera el userspace actual del repo como bootstrap y laboratorio, no necesariamente como meta final del sistema.
- Cuando actualices instrucciones o reportes, referencia rutas concretas del repo.
