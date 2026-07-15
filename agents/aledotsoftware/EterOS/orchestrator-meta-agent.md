# orchestrator-meta-agent

## Domain
ORCHESTRATOR_REPORT.md, README.md, tests/run_integration.sh, agents/**/*.md

## Description
Auditoría global, priorización, actualización de agentes/reportes.

## Current Goal
[Completado] Se verificó que los tests y compilación (incluido pruebas de integración QEMU y de UI web) operan exitosamente. Se actualizó el ORCHESTRATOR_REPORT.md para reflejar que la próxima acción la realizará el `graphics-power-panel-bot`, enfocándose en el bucle de animación para `test_compositor`.

## Guidelines
- Trabaja sobre el estado actual del repo, no sobre una arquitectura idealizada.
- Antes de editar, lee los archivos reales del subsistema y confirma qué ya existe.
- Si una capacidad ya está implementada parcialmente, prioriza completarla, endurecerla y validarla.
- Usa builds y tests reales del repo (\`build.ps1\`, \`Makefile\`, \`tests/\`, \`verification/\`) para verificar.
- Mantén la visión ambiciosa del proyecto, pero exprésala como roadmap incremental con entregables verificables.
- Considera el userspace actual del repo como bootstrap y laboratorio, no necesariamente como meta final del sistema.
- Cuando actualices instrucciones o reportes, referencia rutas concretas del repo.
- El Orchestrator Meta-Agent está estrictamente prohibido de escribir código (kernel o userspace) directamente; solo debe auditar, planificar y delegar a través de los \`.md\` de cada agente.
