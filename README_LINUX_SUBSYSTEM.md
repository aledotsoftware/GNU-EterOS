# Aether Linux Subsystem Roadmap

This document outlines the roadmap to achieve full compatibility between the EterOS kernel and the GNU/Linux userland via the Aether Linux Subsystem layer.

## Objective
To build a functional, robust ABI compatibility layer to directly execute x86_64 ELF Linux binaries (e.g., musl, BusyBox, coreutils) on top of the EterOS kernel.

## Completed Milestones
- **ELF Loading**: Detection of `ELFOSABI_LINUX` within `kernel/fs/elf.c`. Support for `ET_EXEC` and `ET_DYN` (PIE).
- **System Call ABI**: Separate dispatch table for Linux `x86_64` syscall numbers (`syscall_linux_table`).
- **Memory Management (`mmap`)**: Support for `MAP_ANONYMOUS | MAP_PRIVATE` mappings without an active file descriptor, required for dynamic linkers and anonymous memory allocations.
- **TLS Initialization (`arch_prctl`)**: Implementation of `ARCH_SET_FS` and `ARCH_GET_FS` to configure Thread Local Storage (TLS) via `MSR_FS_BASE`.
- **Filesystem & Descriptors**: Core posix functions like `openat`, `readlinkat`, `pread64`, `pwrite64`, etc.
- **Robust Signal Handling**: The kernel scheduler's context switch now correctly injects a signal frame into the user thread's stack upon a pending signal and properly returns via `sys_rt_sigreturn` satisfying the x86_64 System V ABI requirements.

## Roadmap & Gaps

### 1. Full VFS & Procfs Emulation (Short Term)
Linux binaries often parse `/proc` and `/sys` to determine system state.
- **Action**: Enhance EterOS's `procfs` to populate critical files like `/proc/self/exe`, `/proc/meminfo`, and `/proc/cpuinfo` in the format expected by GNU tools.

### 2. Futex and pthreads (Medium Term)
EterOS has basic `futex_wait` and `futex_wake`.
- **Action**: Implement robust PI (Priority Inheritance) futexes and ensure the `clone` syscall (`CLONE_THREAD | CLONE_SIGHAND | CLONE_VM`) behaves identically to Linux for strict `pthread` compatibility.

### 3. Advanced Networking Sockets (Long Term)
EterOS routes sockets via lwIP.
- **Action**: Bridge the remaining missing socket options (`setsockopt`, `getsockopt`, `recvmsg`, `sendmsg` complex flags) between the Linux ABI layer and the lwIP backend.

### 4. Memory Management & Address Space (Long Term)
Complete implementation of `mremap`, robust `mprotect` support across all vma regions, and proper handling of `MAP_SHARED` vs `MAP_PRIVATE` copy-on-write semantics.

### 5. GNU Desktop Environment (Vision)
- Bootstrapping standard display servers (like Wayland or X11) by bridging EterOS's mock DRM subsystem to standard Linux ioctls.
