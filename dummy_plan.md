Great. I will:
1. Modify `sys_mmap` in `syscall.c` to support file-backed `mmap`.
   - Remove `if (!(flags & 0x20)) return -ENODEV;`
   - Read from `fd` if `flags & 0x20` (MAP_ANONYMOUS) is not set.
2. In `syscall.c`, define `syscall_linux_table` and `syscall_native_table`. Use `syscall_linux_table` for `syscall_linux_handler` and `syscall_native_table` for `syscall_native_handler`.
3. In `exceptions.asm`, add `EXCEPTION_STUB 128` (which is `0x80`).
4. In `idt.c`, add `idt_set_gate(0x80, (void*)exception_stub_128, 0xEE);` to `idt_init()`.
5. In `idt.c`, add inside `exception_handler_c`:
   ```c
   if (vector == 0x80) {
       syscall_int80_handler((struct syscall_regs*)regs);
       return;
   }
   ```
6. In `syscall.c`, add `void syscall_int80_handler(struct syscall_regs* regs)` which calls the linux handler with the `int 0x80` ABI. (Or directly calls `syscall_linux_table`).
7. In `include/syscall.h`, declare `syscall_int80_handler`.

Let's test if this plan makes sense and compiles!
