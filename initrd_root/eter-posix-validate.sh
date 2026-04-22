# eterOS POSIX validation bundle
# Run with: run /busybox sh /eter-posix-validate.sh

echo "== eterOS POSIX validation start =="
pwd

echo "-- busybox presence --"
/busybox --list

echo "-- root visibility --"
/busybox ls /

echo "-- writable FS smoke (/data, /tmp) --"
/busybox touch /data/posix_smoke.txt
/busybox chmod 644 /data/posix_smoke.txt
/busybox ls /data
/busybox touch /tmp/posix_tmp.txt
/busybox ls /tmp
/busybox rm /tmp/posix_tmp.txt
/busybox rm /data/posix_smoke.txt

echo "-- syscall suite --"
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

echo "== eterOS POSIX validation done =="
