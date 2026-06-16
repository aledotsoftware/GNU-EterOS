# Android Compatibility Layer Roadmap

## 1. Introduction
This document outlines the roadmap and strategy to run Android user-space components (starting with native Bionic binaries) on top of the EterOS kernel. The overarching goal is not to maintain a separate "Android subsystem," but rather to extend the existing Linux compatibility layer so that it serves both GNU/Linux userland and Android seamlessly.

## 2. Gap Analysis (Linux Compat vs. Android Needs)
While EterOS currently maps a growing subset of x86_64 Linux syscalls, Android (via its standard C library, Bionic, and its init process) relies on specialized IPC mechanisms and memory management primitives that traditional GNU systems do not strictly require:

- **IPC (Binder):** Android's core object-oriented IPC. It relies on the `/dev/binder` character device, memory mapping, and complex ioctls (`BINDER_WRITE_READ`).
- **Memory Management (Ashmem/Memfd):** Anonymous shared memory. Historically managed via `/dev/ashmem` ioctls; modern Android uses `memfd_create`. EterOS currently has a basic `memfd_create` implementation but needs an `ashmem` stub for backward compatibility with older binaries.
- **Linker and Threading (Bionic TLS & Prctl):** Bionic expects thread local storage (TLS) to be configured via `MSR_FS_BASE` and relies heavily on `prctl` (e.g., `PR_SET_VMA` to name memory regions and `PR_SET_NAME` to name threads).
- **System Properties (`/__properties__`):** A shared memory region mapped by init and read by all processes to obtain configuration flags.

## 3. Implementation Strategy

### 3.1. Phase 1: IPC Base & Stubs (Current Focus)
- **Action:** Move basic Binder definitions into standard compatibility headers (`include/linux_compat.h`).
- **Action:** Route `/dev/ashmem` to `shmfs_create_memfd` and `/dev/__properties__` to `devfs` with anonymous mappings for Android properties compatibility.
- **Action:** Implement `sys_prctl` with support for `PR_SET_NAME` and `PR_SET_VMA` to satisfy Bionic's linker and memory allocation routines.
- **Action:** Implement basic memory mapping support for `/dev/binder`, `/dev/ashmem`, and `/dev/__properties__` in `sys_mmap`.

### 3.2. Phase 2: Memory & Threading
- **Action:** Map the Bionic Thread Local Storage (TLS) expectations to EterOS's `sys_arch_prctl`.
- **Action:** Expand `sys_clone` and `futex` implementations to strictly handle Bionic's synchronization and thread ID lifecycle.
- **Action:** Emulate Android properties mapping in memory to allow Bionic to read `ro.*` properties safely.
- **Action:** Upgrade the Binder routing mechanism from the basic static payload-copy shim to proper memory mapping for transaction data transfer.

### 3.3. Phase 3: Binder Routing & Native Binaries
- **Action:** Transition the `/dev/binder` stub into a functional message router inside the kernel, allowing `servicemanager` to start and register services.
- **Action:** Execute pure native Android binaries (C/C++ executables dynamically linked to Bionic) without the Java framework.

### 3.4. Phase 4: Full Framework & SurfaceFlinger
- **Action:** Implement Android's `HWC` (Hardware Composer) expectations via EterOS's existing DRM/KMS subsystem (`dev/dri/card0`).
- **Action:** Launch Android Zygote and System Server.

## 4. Conclusion
By reusing EterOS's `x86_64` syscall table and VFS, Android compatibility can be achieved incrementally without fragmenting the kernel design. The priority is hardening `prctl`, `mmap`, and `futex` before activating the complete Binder transaction engine.
