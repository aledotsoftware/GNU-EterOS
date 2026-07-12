# JAA Global System State

Este archivo contiene el estado compartido entre todos los repositorios gestionados por JAA.
Los agentes pueden leer este estado para entender el contexto de otros proyectos.

## 🚀 ACTIVE MILESTONES
- [JAA] Implementación de Jerarquía de Contexto (.jaa.md global) - **COMPLETADO**
- [JAA] Sistema de Estado Global (system-state.md) - **EN PROCESO**
- [GENERAL] Estandarización de agentes para todos los repositorios.
- [EterOS] Kernel Stability & Boot Hardening - **COMPLETADO** (HAL Abstraction, VFS kmalloc limits, Atomic CPU Halts, Robust Signal Delivery)
- [EterOS] Scheduler, SMP & IPC Basic Stabilization - **COMPLETADO** (Fixed private futex hash and wake isolation logic).
- [EterOS] Android Compatibility Layer - **EN PROCESO** (Binder BR_DEAD_REPLY implemented, ELF PT_TLS allocation supported).
- [EterOS] LibC & Userspace Runtime - **COMPLETADO** (Fixed macro expansion, missing syscall implementations on syscall.h and wrapper additions).
- [EterOS] UI & Graphics Polish - **COMPLETADO** (Fixed tooltip rendering and cursor smearing in marea_shell.c).

## 📝 AGENT NOTES
- **Vision Agent**: Reportando progreso en el diseño premium del dashboard.
- **ErrorGuardian**: Monitoreando logs de error en producción.
- **kernel-stability-boot-bot**: Abstacted CPU halting and interrupts architecture-wide via HAL, prevented stack overflow on vfs path normalization, improved signal delivery upon exceptions.
- **scheduler-smp-ipc-bot**: Stabilized futex logic by properly isolating processes on private futexes using cr3 checking.
- **linux-syscall-compliance-bot**: Solved crashes in `sys_getrusage` by validating pointers and padding struct, correctly bypassed GNU `-ENOSYS` crashes via simple stubs for `sys_syslog`, `sys_getgroups` and `sys_setgroups`. Also correctly implemented `sys_getrandom`.
- **aether-droid-subsystem-bot**: Implemented `BR_DEAD_REPLY` for Binder IPC transactions to dead tasks, and added `PT_TLS` parsing/allocation to the ELF loader to initialize static TLS blocks for Bionic binaries.
- **userspace-libc-posix-bot**: Fixed `SYSCALL_RETURN` macro errant semicolon issues across `userspace/libc/src/syscall.c`, and extended it with missing Linux syscall implementations like `getrusage`, `getrandom`, `getgroups`, `setgroups` and `syslog`, updating `syscall.h` as well.
- **vision-cli-agent**: Fixed cursor smearing with tooltips in `marea_shell.c` by ensuring tooltips are dynamically drawn inside `redraw_all()` instead of `draw_window_chrome()`, allowing `cursor_save_bg()` to work accurately without corruption.
