# eterOS System Calls API

eterOS implements a subset of the Linux x86_64 ABI system calls to provide POSIX-like compatibility for user-space applications.

## Standard POSIX Syscalls

The following standard syscalls are implemented and generally behave according to the POSIX specification:

- `sys_read(int fd, void *buf, size_t count)`
- `sys_write(int fd, const void *buf, size_t count)`
- `sys_open(const char *pathname, int flags, mode_t mode)`
- `sys_close(int fd)`
- `sys_lseek(int fd, off_t offset, int whence)`
- `sys_mmap(...)` and `sys_munmap(...)`
- `sys_fork()` / `sys_clone(...)`
- `sys_execve(const char *pathname, char *const argv[], char *const envp[])`
- `sys_wait4(pid_t pid, int *wstatus, int options, struct rusage *rusage)`
- `sys_exit(int status)`
- `sys_kill(pid_t pid, int sig)`
- `sys_chdir(const char *path)`
- `sys_getcwd(char *buf, size_t size)`

## eterOS Specific / Extended Syscalls

While aiming for Linux compatibility, some syscalls may have specific behaviors or limitations in eterOS:

- `sys_ioctl`: Used extensively for terminal control (`TIOCSCTTY`, `TCGETS`, etc.) and specific device interactions (like framebuffer configuration).
- **Networking**: Handled via lwIP integration. Syscalls like `socket`, `bind`, `connect`, `sendto`, `recvfrom` map directly to lwIP's socket API.
- **Android Compatibility**:
  - `sys_openat` handles `/dev/ashmem` by intercepting it and creating a `shmfs_create_memfd` node.
  - Basic `BINDER_WRITE_READ` ioctls are supported on `/dev/binder` for IPC.

For a complete list of mapped syscalls, refer to the `syscall_linux_table` array in `kernel/arch/x86_64/syscall.c`.
