# linux-syscall-compliance-bot

## Domain
kernel/arch/x86_64/syscall.c

## Description
Cobertura progresiva de syscalls Linux x86_64 con foco en GNU/Linux real.

## Current Goal
Implementar funcionalmente `sys_fsync` (y su variante `sys_fdatasync`) devolviendo un estado válido sin `-ENOSYS`, aprovechando que EterOS opera sobre sistemas de archivos síncronos o en RAM. Asimismo, actualizar `sys_truncate` y `sys_ftruncate` para que deleguen en la función `truncate` del nodo VFS o simulen el truncamiento estableciendo un `node->length` directamente en caso de que el driver subyacente no posea soporte nativo, asegurando que devuelvan 0 o valores seguros sin recurrir a `-ENOSYS`. Modificar el test `test_syscall_linux_coverage.c` pertinentemente sin romper el diseño base para asegurar validación continua de las syscalls implementadas.
## Guidelines
- Trabaja sobre el estado actual del repo, no sobre una arquitectura idealizada.
- Antes de editar, lee los archivos reales del subsistema y confirma qué ya existe.
- Si una capacidad ya está implementada parcialmente, prioriza completarla, endurecerla y validarla.
- Usa builds y tests reales del repo (`build.ps1`, `Makefile`, `tests/`, `verification/`) para verificar.
- Mantén la visión ambiciosa del proyecto, pero exprésala como roadmap incremental con entregables verificables.
- Considera el userspace actual del repo como bootstrap y laboratorio, no necesariamente como meta final del sistema.
- Cuando actualices instrucciones o reportes, referencia rutas concretas del repo.
