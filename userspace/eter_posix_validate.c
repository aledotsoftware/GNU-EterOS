#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

static int run_one(const char *path, char *const argv[]) {
    int pid;
    int st = 0;

    printf("[POSIX] RUN %s\n", path);
    pid = fork();
    if (pid < 0) {
        printf("[POSIX] FAIL fork %s\n", path);
        return 1;
    }
    if (pid == 0) {
        execv(path, argv);
        printf("[POSIX] FAIL exec %s\n", path);
        exit(127);
    }

    if (wait(&st) < 0) {
        printf("[POSIX] FAIL wait %s\n", path);
        return 1;
    }

    if ((st & 0x7f) != 0) {
        printf("[POSIX] FAIL signal %s (status=%d)\n", path, st);
        return 1;
    }
    if (((st >> 8) & 0xff) != 0) {
        printf("[POSIX] FAIL rc %s (rc=%d)\n", path, (st >> 8) & 0xff);
        return 1;
    }

    printf("[POSIX] OK   %s\n", path);
    return 0;
}

int main(void) {
    int fail = 0;

    char *a_syscalls[] = {"/test_syscalls_s1.elf", NULL};
    char *a_epoll[] = {"/test_epoll.elf", NULL};
    char *a_sigalt[] = {"/test_sigaltstack.elf", NULL};
    char *a_sigposix[] = {"/test_signal_posix.elf", NULL};
    char *a_waitid[] = {"/test_waitid.elf", NULL};
    char *a_procfs[] = {"/test_procfs.elf", NULL};
    char *a_ptyjc[] = {"/test_pty_jobcontrol.elf", NULL};
    char *a_shebang[] = {"/test_shebang_exec.elf", NULL};
    char *a_ptinterp[] = {"/test_pt_interp_route.elf", NULL};
    char *a_auxv[] = {"/test_auxv.elf", NULL};
    char *a_dlopen[] = {"/test_dlopen.elf", NULL};

    printf("== eterOS POSIX ELF validation start ==\n");

    fail |= run_one("/test_syscalls_s1.elf", a_syscalls);
    fail |= run_one("/test_epoll.elf", a_epoll);
    fail |= run_one("/test_sigaltstack.elf", a_sigalt);
    fail |= run_one("/test_signal_posix.elf", a_sigposix);
    fail |= run_one("/test_waitid.elf", a_waitid);
    fail |= run_one("/test_procfs.elf", a_procfs);
    fail |= run_one("/test_pty_jobcontrol.elf", a_ptyjc);
    fail |= run_one("/test_shebang_exec.elf", a_shebang);
    fail |= run_one("/test_pt_interp_route.elf", a_ptinterp);
    fail |= run_one("/test_auxv.elf", a_auxv);
    fail |= run_one("/test_dlopen.elf", a_dlopen);

    if (fail) {
        printf("== eterOS POSIX ELF validation: FAIL ==\n");
        return 1;
    }

    printf("== eterOS POSIX ELF validation: PASS ==\n");
    return 0;
}
