# eterOS smoke test runner
# Run with: run /busybox sh /eter-tests.sh

help
pwd

/busybox --list
/busybox uname
/busybox ls /

# Filesystem checks in writable mount
cd /data
pwd
/busybox touch smoke_a.txt
/busybox chmod 644 smoke_a.txt
/busybox cp /README smoke_readme.txt
/busybox wc smoke_readme.txt
/busybox head -n 2 smoke_readme.txt
/busybox ls /data

# tmp checks
/busybox touch /tmp/smoke_tmp.txt
/busybox ls /tmp
/busybox rm /tmp/smoke_tmp.txt
/busybox ls /tmp

# cleanup
/busybox rm /data/smoke_a.txt
/busybox rm /data/smoke_readme.txt
/busybox ls /data

# Syscall sprint1 unit suite
/test_syscalls_s1.elf
/test_epoll.elf
/test_sigaltstack.elf
/test_signal_posix.elf
/test_waitid.elf
/test_procfs.elf
/test_pty_jobcontrol.elf
/test_shebang_exec.elf
/test_pt_interp_route.elf
/test_auxv.elf
/test_dlopen.elf
/eter_posix_validate.elf

echo eter-tests: OK
