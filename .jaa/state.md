## JAA Global System State - Agent Update

- Improved DNS Resolution: `cmd_ota.c` now dynamically resolves update server URLs via `net_gethostbyname` instead of hardcoded IPs.
- Addressed memory leaks in `cmd_ota.c` update subcommand to properly `kfree` partition nodes on error flows.
- Normalized eterOS branding globally across README, orchestrator reports, Web UI, Marea shell, Eterland, and control panel.
- VFS security: Ensure read-only filesystems correctly return -EROFS for create/mkdir (devfs, initrd) to prevent write-attempt panics.
- Userspace: Fixed libc syscall wrappers to correctly handle valid high-memory pointers (like mmap returns) by checking `(unsigned long)ret >= (unsigned long)-4095` instead of `ret < 0`.
- Userspace: Verified `crt0.asm` fully aligns with Linux x86_64 ABI specifications for argc, argv, envp, and auxv setup pushed by the kernel.
- **Network UI Support**: Integrated standard EterOS networking flags (`network_ready`, `current_nic`) into the shell networking utility `cmd_time.c` (`cmd_ntp`). Now correctly outputs diagnostics rather than simply 'Network disabled'.
