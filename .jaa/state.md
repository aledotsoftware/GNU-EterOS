## EterOS OTA Update (Current Run)
- Implemented a synchronous DNS resolver `net_gethostbyname` in `kernel/net/compat.c` using `dns_gethostbyname` from lwIP, properly handling timeouts and orphaned state callbacks to prevent use-after-free vulnerabilities.
- Added `sys_lwip_close` helper in `kernel/net/socket.c` to gracefully decrement the VFS node reference count and clean up lwIP sockets instead of leaking them via legacy code.
- Refactored `kernel/shell/cmd_ota.c` to remove legacy `net_*` calls in favor of the new `sys_lwip_*` ecosystem (sockets, connections, sends, receives).
- Refactored `cmd_ota.c` to use `net_gethostbyname` for dynamic OTA server address resolution instead of relying on hardcoded IPs.
- Modified A/B partition logic in `kernel/drivers/disk/partition.c` (`partition_get_active_root` and `partition_get_passive_root`) to use `nvram_get_boot_partition()` as the source of truth, moving away from relying on the MBR `boot_indicator`.

# JAA Context State

## EterOS Aether Linux Subsystem (Previous Run)
- Hardened `kernel/fs/elf.c` to prevent string bounds checking bypasses and buffer overflows during `PT_INTERP` extraction by safely capping `out_interp` size.
- Hardened `kernel/arch/x86_64/syscall.c` `sys_mmap` to automatically add `MAP_PRIVATE` for ABI compatibility when no mapping flags are provided by Linux binaries.
- Refactored `sys_arch_prctl` to correctly read `MSR_FS_BASE` and `MSR_KERNEL_GS_BASE` for `ARCH_GET_FS` and `ARCH_GET_GS`, copying safely to userspace using `vmm_verify_user_access`.
- Extented `sys_rt_sigaction` to support up to 64 signals instead of 31 for full `sigset_t` compliance.
- Fixed `sys_rt_sigprocmask` to use 64-bit masks by using `1ULL` shifts to avoid undefined behavior overflow.
- Secured `sys_openat` with explicit `vmm_verify_user_access` boundary checks.
- Added explicit NUL-termination for `sys_readlinkat` when the read size is strictly smaller than the requested buffer.

## EterOS Scheduler & IPC Update (2024-04-24)
- Resolved a critical bug in `kernel/task.c:schedule()` where tasks selected to run again without switching context were not removed from the ready queue if they had been previously enqueued, corrupting the ready list.
- Addressed thread-safety issues in state transitions within `kernel/futex.c:futex_wait()` and `kernel/sem.c:sem_wait()` by introducing and utilizing a new locked `task_block()` API.
- Fixed `task_sleep()` by removing an arbitrary uninterruptible `hlt` busy wait.
- Tested and verified fixes via host-based unit tests (`test_sem`, `test_futex_logic`, `test_futex_timeout`) and a QEMU headless boot, ensuring Ring 3 interactions (`login.elf`) execute robustly under corrected load.

## Android Subsystem Compatibility Update (2024-04-23)
- Conducted gap analysis between EterOS Linux compatibility and Android (Bionic/Linker) expectations.
- Implemented `/dev/binder` stub in `kernel/fs/devfs.c` supporting the `BINDER_VERSION_IOWR` ioctl response.
- Implemented Linux native `sys_memfd_create` (syscall 319) in `kernel/arch/x86_64/syscall.c` leveraging anonymous Shared Memory nodes (`shmfs`).
- Modified `shmfs_close` to safely release anonymous shared memory pages when the open file descriptor count hits zero.
- Re-verified full kernel compilation (`make clean && make all`) and successfully passed all native host VFS/Syscall C tests.
