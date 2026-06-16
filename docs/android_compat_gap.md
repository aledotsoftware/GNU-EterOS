# Android Compatibility Gap Analysis on EterOS

## 1. Overview
EterOS has a growing Linux ABI compatibility layer capable of running standard x86_64 GNU/Linux binaries. However, Android's Bionic libc and userland rely on specific Linux features that go beyond POSIX and standard GNU expectations. This document outlines the gaps and the strategy to support native Android binaries on the EterOS kernel.

## 2. Gaps and Requirements

### 2.1 IPC: Binder
**Current State:** EterOS supports basic POSIX IPC, futexes, and shared memory via `/tmp` (shmfs).
**Android Need:** Android heavily relies on Binder for inter-process communication.
**Strategy:** Implement a `/dev/binder` driver in `devfs.c` that handles basic `ioctl` commands (`BINDER_VERSION`, `BINDER_WRITE_READ`, `BINDER_SET_CONTEXT_MGR`), `mmap`, and thread management to support `servicemanager`.

### 2.2 Memory Management: Ashmem / Memfd
**Current State:** `shmfs` handles both `memfd_create` and legacy `/dev/ashmem` mappings dynamically via `sys_openat` interception.
**Android Need:** Historically used `ashmem` (`/dev/ashmem`), but modern Android (API 29+) relies heavily on `memfd_create`.
**Strategy:** Implement `sys_memfd_create` natively, backing it with the existing `shmfs` infrastructure, providing seamless anonymous file-backed memory sharing. Support sealing (`MFD_ALLOW_SEALING`) progressively.

### 2.3 System Properties
**Current State:** No centralized property service.
**Android Need:** `init` and `bionic` require the Android property tree, usually backed by shared memory (`/dev/__properties__`).
**Strategy:** Implement the property service as a user-space daemon or leverage a dedicated `shmfs` region for system properties accessible via standard `mmap`.

### 2.4 Signals and Threading (Bionic / pthreads)
**Current State:** `futex`, `clone`, `sigaction` (up to 64 signals), and `sigprocmask` are partially implemented.
**Android Need:** Bionic threads depend heavily on `clone` with `CLONE_CHILD_CLEARTID`, `CLONE_CHILD_SETTID`, robust futexes, and thread-local storage (`MSR_FS_BASE` via `arch_prctl`).
**Strategy:** Ensure `sys_clone` fully supports TLS setup and PID/TID tracking. Harden `futex` to handle `FUTEX_WAIT_BITSET` and `FUTEX_WAKE_BITSET`.

### 2.5 VFS and Linker Expectations
**Current State:** `initrd`, `fat32`, `jfs`, `shmfs`, `devfs`, `procfs`.
**Android Need:** Bionic's linker (`/system/bin/linker64`) expects precise `mmap` behavior (e.g., `MAP_FIXED` over existing mappings) and access to `/proc/self/maps`.
**Strategy:** Expand `/proc` with `self/maps` and `self/exe`. Ensure `sys_mmap` gracefully handles overlapping `MAP_FIXED` calls for ELF loading.

## 3. Initial Milestones
1. **Foundation:** Implement `memfd_create` and basic `/dev/binder` node.
2. **Runtime:** Successfully load Bionic's `linker64` and a statically linked Android `hello-world`.
3. **IPC:** Boot a basic `servicemanager` and perform a Binder transaction.
4. **Memory:** Successfully mapped `/dev/binder`, `/dev/ashmem`, and `/dev/__properties__` through `sys_mmap` with fallback to anonymous mapping for Android compatibility.

### 2.6 Binder Memory Mapping (Future Work)
**Current State:** A rudimentary Binder routing engine exists that copies payloads into a static kernel buffer, returning them to the context manager. Additionally, anonymous receive buffers are mapped directly into processes' virtual address space.
**Strategy:** Transition to a true Binder memory model where `mmap` is used to map a shared read-only buffer into the receiver's address space. The kernel will allocate physical pages and map them into this VMA, copying the transaction data directly from the sender's user space into the receiver's mapped buffer, avoiding double-copying and static limits.
