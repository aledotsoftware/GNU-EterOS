# Android on EterOS Roadmap & Gap Analysis

## 1. Gap Analysis: Current Linux Compat vs. Android Needs

EterOS currently possesses a functional base for standard Linux (POSIX) syscalls. However, Android's ecosystem introduces several unique components that diverge from standard GNU/Linux.

### Syscalls & Core IPC
*   **Linux (EterOS Current):** `sys_mmap`, signals, standard BSD sockets, basic `futex`, and file I/O operations.
*   **Android Gap:** Android heavily relies on `Binder` for all framework IPC, and `ashmem` (or `memfd_create`) for shared memory. EterOS has a barebones, stubbed Binder driver that requires significant hardening and transactional depth to support full Android userland expectations.

### TLS and Memory Management
*   **Linux (EterOS Current):** Basic `ARCH_SET_FS` for thread-local storage mapping. `kmalloc` and physical page mmap allocation.
*   **Android Gap:** Bionic (Android's libc) enforces strict thread-local memory layouts via `prctl` (specifically `PR_SET_VMA` for memory naming). EterOS needs robust thread stack bounds tracking, mapping protections, and memory region tagging to satisfy Bionic's linker.

### Userspace Linker (Bionic)
*   **Linux (EterOS Current):** A simple `ELF` loader that primarily runs static binaries or simplistic `libc` variants.
*   **Android Gap:** The `/system/bin/linker64` expects `PT_INTERP` dynamic linking support, complex VMA layout with `MAP_FIXED` rules, and property mappings (`/dev/__properties__`).

## 2. Binder Design & Implementation Strategy

Binder must be integrated safely within the existing EterOS VFS structure, reusing the Linux-compatible memory management layer.

*   **VFS Integration:** `/dev/binder` implemented in `devfs.c`.
*   **Memory Management (`mmap`):** Rather than Binder allocating its own raw slabs, it hooks into `sys_mmap` using the `is_binder` pathway. The VMM sets up read-only mappings in the target process. Binder allocations use `temp_payload` and copy directly into the VMA area during `BC_TRANSACTION`.
*   **Cleanup (`munmap`):** `sys_munmap` must detect the Binder region bounds and clear the tracked fields (`binder_mmap_base`, `binder_mmap_kptr`) inside the `task_t` struct to prevent leaks. *(Recently patched)*.
*   **Next Steps for Binder:** Implement proper reference counting (`BR_ACQUIRE`/`BR_RELEASE`), robust buffer freeing (`BC_FREE_BUFFER` logic hardening), and full context-manager (`servicemanager`) state tracking.

## 3. Strategy for Ashmem, Memfd, and Properties

### Ashmem & Memfd
Android historically used `/dev/ashmem` but migrated to `memfd_create`. EterOS will bridge this by using `shmfs` (Shared Memory FS).
*   `/dev/ashmem` opens dynamically dispatch to `shmfs_create_memfd()`.
*   `ASHMEM_SET_NAME` and size/protection IOCTLs are routed directly to `shmfs` handlers.
*   EterOS will prioritize `memfd_create` compliance (syscall 319) natively, aliasing `ashmem` over it for legacy Android binaries.

### Properties (`/dev/__properties__`)
Bionic requires system properties for initialization.
*   EterOS will provide a stubbed memory-mapped file containing essential `ro.build.version.sdk` key-value pairs to unblock initial native binary execution.

## 4. Execution Roadmap (Iterative Phasing)

1.  **Phase 1: Environment & Primitive Hardening (Current)**
    *   Solidify `binder` VMA `mmap`/`munmap` memory management.
    *   Fix fallback kmalloc memory allocation bugs in `devfs.c` (completed).
    *   Implement basic Binder transactions for `ping` tests.
2.  **Phase 2: Android Native Core Utilities**
    *   Support Bionic static binaries (`toolbox`/`toybox`).
    *   Implement missing `prctl` sub-functions (VMA naming).
    *   Provide dummy `/dev/__properties__` mappings.
3.  **Phase 3: Linker & Dynamic Loading**
    *   Upgrade EterOS ELF loader to support `PT_DYNAMIC` and `PT_INTERP`.
    *   Boot `linker64` and execute dynamically linked native Android tests.
4.  **Phase 4: Framework Initialization**
    *   Boot `servicemanager` (requires hardened Binder reference tracking).
    *   Boot `init` and `logd`.
