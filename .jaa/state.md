## JAA Global System State - Agent Update

- Improved DNS Resolution: `cmd_ota.c` now dynamically resolves update server URLs via `net_gethostbyname` instead of hardcoded IPs.
- Addressed memory leaks in `cmd_ota.c` update subcommand to properly `kfree` partition nodes on error flows.
- Normalized eterOS branding globally across README, orchestrator reports, Web UI, Marea shell, Eterland, and control panel.
- VFS security: Ensure read-only filesystems correctly return -EROFS for create/mkdir (devfs, initrd) to prevent write-attempt panics.
- Userspace: Fixed libc syscall wrappers to correctly handle valid high-memory pointers (like mmap returns) by checking `(unsigned long)ret >= (unsigned long)-4095` instead of `ret < 0`.
- Userspace: Verified `crt0.asm` fully aligns with Linux x86_64 ABI specifications for argc, argv, envp, and auxv setup pushed by the kernel.
- **Network UI Support**: Integrated standard EterOS networking flags (`network_ready`, `current_nic`) into the shell networking utility `cmd_time.c` (`cmd_ntp`). Now correctly outputs diagnostics rather than simply 'Network disabled'.
\n- System: Consolidated multi-user flow by prepopulating default /etc/passwd, /etc/shadow, and /etc/autologin in the initrd and automatically mirroring them to the writable /etc shmfs mount. Addressed memory leaks (truncate) and enforced shadow file 0600 permissions in kernel shell utilities.
- **Orchestrator**: Verified DRM framebuffer mmap and ELF PT_DYNAMIC support. Assigned lwIP and socket syscalls expansion to network-socket-api-bot.
