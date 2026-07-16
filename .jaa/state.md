# JAA Global System State

Este archivo contiene el estado compartido entre todos los repositorios gestionados por JAA.
Los agentes pueden leer este estado para entender el contexto de otros proyectos.

## 🚀 ACTIVE MILESTONES
- [JAA] Implementación de Jerarquía de Contexto (.jaa.md global) - **COMPLETADO**
- [JAA] Sistema de Estado Global (system-state.md) - **EN PROCESO**
- [AETHER] Implementada emulación parcial de /proc/cpuinfo en procfs para compatibilidad con GNU/Linux. - **COMPLETADO**
- [EterOS] System V ABI Compliance - [AETHER] Implementada emulación parcial de /proc/cpuinfo en procfs para compatibilidad con GNU/Linux. - **COMPLETADO** Subsystem Linux Hardening - **COMPLETADO** (Signal handling stack alignment, sys_mremap COW preservation, MSR swap restoration fix).
- [GENERAL] Estandarización de agentes para todos los repositorios.
- [EterOS] Kernel Stability & Boot Hardening - **COMPLETADO** (HAL Abstraction, VFS kmalloc limits, Atomic CPU Halts, Robust Signal Delivery)
- [EterOS] Scheduler, SMP & IPC Basic Stabilization - **COMPLETADO** (Fixed private futex hash and wake isolation logic).
- [EterOS] Android Compatibility Layer - **EN PROCESO** (Binder BR_DEAD_REPLY implemented, ELF PT_TLS allocation supported).
- [EterOS] LibC & Userspace Runtime - **COMPLETADO** (Added full missing definitions on system headers such as sys/resource.h, sys/random.h, sys/msg.h, sys/shm.h, syslog.h, grp.h, fixed malloc assertions on 16 byte alignments, added wait macros).
- [EterOS] VFS, Initrd, ProcFS y Carga de Binarios - **COMPLETADO** (Atomic rename mechanics implemented in FAT32 and JFS, POSIX semantics for EEXIST on fat32 creation fixed, POSIX semantics for read-only unlink/rename in initrd/procfs/devfs implemented).
- [EterOS] UI & Graphics Polish - **COMPLETADO** (Fixed tooltip rendering and cursor smearing in marea_shell.c).
- [EterOS] Devices, Time & Control Panel - **COMPLETADO** (Improved panel UI coordinate mappings, fixed NTP timestamp calculations, moved to standard irq_save/restore in input drivers, cleaned up dependencies).
- [EterOS] OTA Update & A/B Slots - **COMPLETADO** (Hardened Ed25519 signature validation, enforced strictly 0 or 1 slot logic, and fixed simulation rollback states).

## 📝 AGENT NOTES
- **graphics-power-panel-bot**: Implementado el prototipo del compositor visual animado (`test_compositor`) en `kernel/shell/cmd_misc.c` utilizando `window_invalidate`, `compositor_render` y `timer_sleep`. Esto permite verificar el sistema gráfico con repintado animado hacia un GNU Desktop sobre EterOS.
- **vfs-posix-filesystem-bot**: Implemented `unlink` and `rename` operations returning `-EROFS` in read-only filesystems (`initrd.c`, `procfs.c`, `devfs.c`). Updated `vfs.c` and `sys_rename` to correctly fallback to `-EPERM` when a filesystem (like read-only ones) lacks these function implementations on a directory instead of generic ENOTDIR or ENOSYS. Cleaned up scratchpad bash scripts.
- **ota-update-panel-bot**: Updated `pack_payload.py` to use real PyNaCl Ed25519 signatures, matched the keypair to `cmd_ota.c`, hardened `partition_get_passive_root` to strictly enforce 0 or 1 slot indices to prevent overwriting data partitions, and fixed the simulation rollback tests.
- **Orchestrator Meta-Agent**: Auditó y priorizó el ciclo actual hacia `graphics-power-panel-bot` para reanudar el desarrollo del compositor UI (`test_compositor`) después de verificar que el `userspace-libc-posix-bot` logró cobertura POSIX estable en la API de libc. Tests verificados.
- **Vision Agent**: Reportando progreso en el diseño premium del dashboard.
- **ErrorGuardian**: Monitoreando logs de error en producción.
- **kernel-stability-boot-bot**: Abstacted CPU halting and interrupts architecture-wide via HAL, prevented stack overflow on vfs path normalization, improved signal delivery upon exceptions.
- **kernel-stability-boot-bot**: Refactored x86_64, ARM Cortex-M, Xtensa, and RISC-V architectures to use atomic `hal_cpu_enable_interrupts_and_halt()` instead of separate `hal_interrupts_enable()` and `hal_cpu_halt()` to prevent lost wakeups. Cleaned up inline assembly outside of HAL, verified memory mapping with `hal_mem_map()` instead of lower-level mappings, and checked robust signal delivery with fully populated `syscall_regs`.
- **scheduler-smp-ipc-bot**: Stabilized futex logic by properly isolating processes on private futexes using cr3 checking.
- **linux-syscall-compliance-bot**: Solved crashes in `sys_getrusage` by validating pointers and padding struct, correctly bypassed GNU `-ENOSYS` crashes via simple stubs for `sys_syslog`, `sys_getgroups` and `sys_setgroups`. Also correctly implemented `sys_getrandom`. Added numerous `sys_` stubs (msync, mincore, shm, msg, sem, etc.) to the `syscall_linux_table` and expanded libc wrappers to further improve Linux x86_64 ABI compatibility and progressively cover the syscall table without crashing GNU userland probing.
- **aether-linux-subsystem-bot**: Enforced System V ABI alignment `16n+8` in `setup_sigcontext`, fixed physical page remapping in `sys_mremap` by removing `sys_munmap` which breaks COW memory, and fixed restoration of user_stack_scratch after handling signals correctly from `syscall_entry.asm`.
- **userspace-libc-posix-bot**: Fixed `SYSCALL_RETURN` macro errant semicolon issues across `userspace/libc/src/syscall.c`, extended it with missing Linux syscall wrappers, fixed tests to use proper malloc alignment of 16 bytes for host tests and added missing headers implementations such as `<sys/resource.h>`, `<sys/shm.h>`, `<sys/msg.h>`, `<sys/ptrace.h>`, `<syslog.h>`, `<grp.h>` matching the stubs, achieving full integration and POSIX verification tests passing on userspace. Fully implemented syslog to expand arguments correctly via vsnprintf.
- **vision-cli-agent**: Fixed cursor smearing with tooltips in `marea_shell.c` by ensuring tooltips are dynamically drawn inside `redraw_all()` instead of `draw_window_chrome()`, allowing `cursor_save_bg()` to work accurately without corruption.
- **devices-time-panel-bot**: Refactored `kernel/drivers/input/input.c` to use robust HAL functions `irq_save` and `irq_restore` rather than raw inline assembly. Fixed a scoping issue in `kernel/shell/cmd_devices.c` by moving `#include <nvram.h>` to the global scope. Fixed an off-by-one mapping for the time control panel interaction bounds in `kernel/shell/cmd_panel.c` and updated `cmd_time.c` to explicitly cast 32-bit `ntp_secs` to `uint64_t` before applying the 64-bit `NTP_TIMESTAMP_DELTA` to avoid overflow edge cases. All Python verifications and `test_build.sh` successful.
- **aether-droid-subsystem-bot**: Generated ANDROID_ROADMAP.md defining gap analysis and iterative execution plan for Android compatibility. Fixed syntax errors in binder fallback allocation, and safely integrated binder memory release into sys_munmap to prevent VMA leaks.
