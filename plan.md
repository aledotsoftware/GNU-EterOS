1. **Fix `userspace/login.c`**: Update `execve` calls to explicitly pass `envp` with `USER`, `HOME`, `PATH`, and `TERM` instead of `NULL`.
2. **Fix `kernel/shell/cmd_user.c`**: Add `vfs_unlink` for temporary files (`shadow.tmp` and `passwd.tmp`) after they are processed to prevent memory leaks in RAM-backed FS.
3. **Pre-commit checks**: Complete pre-commit steps to ensure proper testing, verification, review, and reflections are done.
4. **Submit**: Submit the changes.
