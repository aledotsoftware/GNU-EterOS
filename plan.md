1. **syscall() Wrapper Genérico**:
   - Use `replace_with_git_merge_diff` on `userspace/libc/src/syscall.c` to add `long syscall(long nr, ...)`. This wrapper will match Linux's convention and set `errno`.
   - Update `userspace/libc/include/sys/syscall.h` to declare `long syscall(long nr, ...);`.

2. **Verify `syscall.c`**:
   - Run `cd userspace && make libc/src/syscall.o` via `run_in_bash_session` to verify it compiles.

3. **Update `stdio.h` & `stdio.c` (Buffered I/O)**:
   - Use `replace_with_git_merge_diff` on `userspace/libc/include/stdio.h` to add missing declarations (`fileno`, `feof`, `ferror`, `getc`, `putc`, `ungetc`).
   - Use `replace_with_git_merge_diff` on `userspace/libc/src/stdio.c` to implement `fileno`, `feof`, `ferror`, `getc`, `putc`, `ungetc`.

4. **Verify `stdio.c`**:
   - Run `cd userspace && make libc/src/stdio.o` to verify.

5. **Implement `stdlib.h` functions**:
   - Use `replace_with_git_merge_diff` on `userspace/libc/src/stdlib.c` to add implementations for `strtol`, `strtoul`, `abs`, `qsort`, `bsearch`, `getenv`, `setenv`, `atexit`, `system`. `atoi` and `abort` are already implemented.
   - Update `userspace/libc/include/stdlib.h` if declarations are missing for `system` or `atexit`.

6. **Verify `stdlib.c`**:
   - Run `cd userspace && make libc/src/stdlib.o`.

7. **Implement `string.h` extensions**:
   - Use `replace_with_git_merge_diff` on `userspace/libc/src/strtok.c` (or a new file/string.c) to implement `strtok_r`.
   - Use `replace_with_git_merge_diff` on `userspace/libc/src/string.c` to implement `strdup`, `strsep`, `strpbrk`, `strncasecmp`, `memrchr`. `strerror` is already there.

8. **Verify `string.c`**:
   - Run `cd userspace && make libc/src/string.o`.

9. **Update `Makefile`**:
   - Use `replace_with_git_merge_diff` on `userspace/Makefile` to add `libc/src/pthread.o` and `libc/src/dirent.o` to `LIBC_OBJ` (by adding them to `LIBC_SRC` list).

10. **Implement pthreads**:
    - Use `write_file` to create `userspace/libc/src/pthread.c` with implementations for `pthread_create`, `pthread_join`, `pthread_exit`, `pthread_self`, `pthread_detach`, `pthread_mutex_init`, `pthread_mutex_lock`, `pthread_mutex_unlock`, `pthread_mutex_destroy`, `pthread_cond_init`, `pthread_cond_wait`, `pthread_cond_signal`, `pthread_cond_broadcast`.

11. **Verify `pthread.c`**:
    - Run `cd userspace && make libc/src/pthread.o`.

12. **Implement `signal.h` functions**:
    - Use `replace_with_git_merge_diff` on `userspace/libc/src/signal.c` to implement `sigemptyset`, `sigfillset`, `sigaddset`. `signal`, `sigaction`, `sigprocmask`, `raise`, `kill` are already partially there or can be improved if missing.

13. **Verify `signal.c`**:
    - Run `cd userspace && make libc/src/signal.o`.

14. **Implement `time.h` functions**:
    - Use `replace_with_git_merge_diff` on `userspace/libc/src/time.c` to implement missing functions (`localtime`, `gettimeofday`, `sleep`). `time`, `clock_gettime`, `nanosleep`, `gmtime`, `strftime` are mostly there.

15. **Verify `time.c`**:
    - Run `cd userspace && make libc/src/time.o`.

16. **Update `crt0.asm`**:
    - Use `replace_with_git_merge_diff` on `userspace/libc/src/crt0.asm` to correctly parse `argc`, `argv`, `envp`, `auxv`.

17. **Verify `crt0.asm`**:
    - Run `cd userspace && nasm -f elf64 libc/src/crt0.asm -o libc/src/crt0.o` (assuming nasm is used for asm files, let me check build system. Actually crt0.asm is not in Makefile, we might need to check if it's built elsewhere).

18. **Implement `dirent.c`**:
    - Use `write_file` to create `userspace/libc/src/dirent.c` implementing `opendir`, `readdir`, `closedir` via `SYS_getdents64`.

19. **Verify `dirent.c`**:
    - Run `cd userspace && make libc/src/dirent.o`.

20. **Verify integration**:
    - Run `cd userspace && make clean && make all` via `run_in_bash_session` to ensure everything links properly.

21. **Pre-commit step**: Complete pre-commit steps to ensure proper testing, verification, review, and reflection are done.
