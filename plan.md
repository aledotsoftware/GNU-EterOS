# Plan

1. **Implement `syscall()` wrapper generic**
   - Create `userspace/libc/src/syscall_wrap.c` with a generic `long syscall(long nr, ...)` function.
   - The wrapper should call the standard Linux x86_64 syscall interface, checking for return values `-1..-4095` to set `errno` and return `-1`.
   - Update `userspace/libc/include/unistd.h` (or a suitable header) to declare `syscall`.

2. **Enhance `stdio.h` and `stdio.c` for Buffered I/O**
   - Redefine `struct _eteros_IO_FILE` in `userspace/libc/src/stdio.c` (or a private header) to include an 8KB buffer, a read/write buffer position, mode flags (read, write, append), and the `fd`.
   - Implement buffered I/O functions: `fopen`, `fclose`, `fflush`, `fread`, `fwrite`, `fgets`, `fputs`, `fseek`, `ftell`, `feof`, `ferror`, `fileno`, `getc`, `putc`, `ungetc`, `perror`. Use the existing syscall wrappers (`open`, `read`, `write`, `close`, `lseek`).
   - Implement `fprintf`.

3. **Enhance `stdlib.h` and `stdlib.c`**
   - Implement `atoi` (already present, need to check), `strtol`, `strtoul`, `abs`, `qsort`, `bsearch`.
   - Implement environment functions: `getenv`, `setenv`, `atexit`, `exit` (needs to run atexit handlers), `abort` (already present), `system` (stub or execve wrapper).

4. **Enhance `string.h` and `string.c`**
   - Implement `strdup`, `strtok`, `strtok_r`, `strsep`, `strpbrk`, `strerror`, `strncasecmp`, `memrchr`.

5. **Implement pthreads over `clone` and `futex`**
   - Add `userspace/libc/src/pthread.c`.
   - Implement `pthread_create` using `clone`.
   - Implement `pthread_join`, `pthread_exit`, `pthread_self`, `pthread_detach`.
   - Implement `pthread_mutex_init`, `pthread_mutex_lock`, `pthread_mutex_unlock`, `pthread_mutex_destroy` using `futex`.
   - Implement `pthread_cond_init`, `pthread_cond_wait`, `pthread_cond_signal`, `pthread_cond_broadcast` using `futex`.

6. **Enhance `signal.h` and `signal.c`**
   - Add implementations for `sigemptyset`, `sigfillset`, `sigaddset`.
   - Check `signal`, `sigaction`, `sigprocmask`, `raise`, `kill` which are mostly present but might need verification against requirements.

7. **Enhance `time.h` and `time.c`**
   - Implement `localtime`, `strftime`.
   - Verify `time`, `clock_gettime`, `gettimeofday`, `nanosleep`, `sleep`, `gmtime`.

8. **Fix `crt0.asm`**
   - We already created `crt0.asm` parsing `argc`, `argv`, `envp`.
   - Need to parse `auxv` (Auxiliary Vector) and call `_init` array if any, though the instructions mention "Parse argc/argv/envp/auxv from stack. Call constructors -> main() -> exit()". Add constructors call.

9. **Headers (Stubs)**
   - Created `<dirent.h>`, `<sys/socket.h>`, `<poll.h>`, `<dlfcn.h>`, `<locale.h>`.
   - Need `<netinet/in.h>`, `<arpa/inet.h>`.
   - Implement `<dirent.h>` functions (`opendir`, `readdir`, `closedir`) using `getdents64`.

10. **Pre-commit and Verifications**
    - Compile with `make` and test in sandbox. Check if it compiles cleanly.
    - Follow `pre_commit_instructions` tool to complete pre commit steps ensuring proper testing, verifications, reviews and reflections are done.
    - Submit PR.
