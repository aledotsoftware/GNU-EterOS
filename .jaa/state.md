# JAA Global System State - Agent Update

- **graphics-power-panel-bot**: Graphical UI and Window Controls Tooltips updated.
  - Implemented CSS tooltips using `::before` pseudo-element for `.control` class in `web_ui/style.css` (Cerrar, Minimizar, Maximizar window controls) providing consistency across the web UI.
  - Aligned native C `marea_shell.c` by drawing hover tooltips directly in `draw_window_chrome()` based on hit detection logic (`hit_minimize_button`, etc.), matching web UI tooltips logic in the native stack.

- **kernel-stability-boot-bot**: Boot, memory, and initialization hardened and verified.
  - Verified PMM bitmap is safely pinned at `0x1A000`.
  - Confirmed `total_pages` boundary constraints are active in `pmm_mark_region_used`.
  - Verified `vmm_virt_to_phys` is used for stack pointer validation during panics in `idt.c` to prevent nested faults.
  - Confirmed `ist[1]` isolation is maintained in `tss_set_rsp0` for Double Fault protection.
  - Removed stale `.rej` artifacts and restored robust QA build flow (`nasm` dependency resolved).
  - Hardened `handle_exception` in `kernel/arch/x86_64/idt.c` to output full register traces unconditionally on unhandled exceptions.
  - Hardened architectural boundaries in `smp.c` and `task.c` by substituting raw `cli`/`sti` instructions with cross-platform `hal_interrupts_disable`/`hal_interrupts_enable` abstractions.
No changes were needed for the task as network integration is fully functional
- **linux-syscall-compliance-bot**: Checked `sys_fsync`, `sys_fdatasync`, `sys_truncate`, and `sys_ftruncate` implementations. They were already compliant. Fixed their tests in `test_syscall_linux_coverage.c` to prevent false failures.
