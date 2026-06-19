# Android Compatibility Gap Analysis

## 1. Binder IPC
- **Current State**: Initial Binder IPC implementation (`BINDER_WRITE_READ`) is present in `kernel/fs/devfs.c`. It routes requests to a context manager and queues transactions.
- **Gaps**:
  - Requires further hardening, possibly memory-mapping support (`mmap` on `/dev/binder`).
  - Need support for BINDER_SET_MAX_THREADS and other ioctls.
  - Proper fd handling and process death notifications (`BR_DEAD_REPLY`).

## 2. Memory Management (Ashmem / Memfd)
- **Current State**: `/dev/ashmem` is redirected to `shmfs_create_memfd` in `sys_openat`. `ASHMEM_*` ioctls are implemented in `shmfs_ioctl`.
- **Gaps**:
  - `ashmem` usually requires pin/unpin support (currently stubbed or unhandled in shmfs).
  - Need to verify mapping behavior (MAP_SHARED).

## 3. Linker & Threading (Bionic)
- **Current State**:
  - `sys_socketpair` implemented via `sys_pipe2` wrapper.
  - `prctl` implemented (handles PR_SET_VMA, PR_SET_NAME).
  - `arch_prctl` (ARCH_SET_FS) implemented for TLS.
  - Futexes have basic `FUTEX_WAIT`, `FUTEX_WAKE`, and `BITSET` variants.
  - `PT_DYNAMIC` parsed in ELF loader.
- **Gaps**:
  - `PT_TLS` (Thread Local Storage segment) is completely missing from the ELF loader (`kernel/fs/elf.c`). Bionic heavily relies on `PT_TLS` for thread-local variables.
  - `clone` with `CLONE_THREAD | CLONE_SIGHAND | CLONE_VM` needs verification for proper Bionic pthreads.
  - `sys_mprotect` may need Android-specific tweaks (PROT_EXEC permissions handling).

## 4. Properties System
- **Current State**: `/dev/__properties__` mapped as an anonymous page in `sys_mmap` with dummy data `ro.build.version.sdk=29`.
- **Gaps**:
  - Proper tree/dictionary structure for properties.
  - Read/Write semantics and property service.

## Strategy for Next Steps
1. **ELF Loader PT_TLS**: Implement `PT_TLS` segment parsing in `kernel/fs/elf.c` to support TLS block allocation and initialization for Bionic and GNU libc.
2. **Binder Mmap**: Implement `mmap` for `/dev/binder` to handle transaction buffers directly via VMA, replacing the current `kmalloc` / `anonymous page` workaround.
