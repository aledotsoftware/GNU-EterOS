# eterOS Filesystem Details

This document provides in-depth details on the specific filesystems supported by the eterOS VFS.

## DevFS (Device Filesystem)

Located at `/dev`, this virtual filesystem provides access to hardware and pseudo-devices.

- **`/dev/null`**: Discards all writes; reads return EOF.
- **`/dev/zero`**: Writes are discarded; reads return infinite null bytes.
- **`/dev/tty`**: Represents the controlling terminal for the current process. Dynamically routes to the appropriate underlying PTY or serial port.
- **`/dev/urandom`**: Provides cryptographically secure pseudorandom numbers.
- **`/dev/ashmem`**: Android shared memory allocator pseudo-device. Opening it creates a new shared memory region (internally routed to ShmFS).
- **`/dev/binder`**: Android Binder IPC pseudo-device. Implements ioctls for message routing between processes.

## ProcFS (Process Filesystem)

Located at `/proc`, this read-only filesystem exposes kernel and system state to userspace.

- **`/proc/uptime`**: System uptime in seconds.
- **`/proc/version`**: Kernel version string and build details.
- **`/proc/meminfo`**: Memory utilization statistics (Total, Free, Used RAM).
- **`/proc/[pid]/`**: Future expansion: directories containing state for individual running tasks.

## InitRD (Initial RAM Disk)

Mounted at `/` (root), this is a read-only filesystem loaded into memory by the bootloader alongside the kernel.

- **Format**: It uses a simple, custom packed format or uncompressed TAR. The kernel parses the headers (`initrd_file_header_t`) to locate files.
- **Usage**: Contains essential early-boot binaries (like `sh.elf`, `login.elf`) and configuration files (`/etc/passwd`, `/etc/shadow`) necessary before real disk drivers are initialized.

## JFS (Journaling Filesystem)

An experimental filesystem designed for data integrity.

- **Features**: Implements true atomic multi-block commits to ensure metadata consistency in the event of a power loss or crash.
- **Hardlinks**: Supports POSIX hardlinks via `sys_linkat`, allowing multiple directory entries to point to the same underlying inode.

## ShmFS (Shared Memory Filesystem)

Mounted at `/dev/shm/`, used for POSIX shared memory (`shm_open`) and backing anonymous `mmap` allocations. Operations here directly allocate and manage physical pages that can be shared across multiple process address spaces.
