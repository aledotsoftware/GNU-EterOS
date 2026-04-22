#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

static int append_uint(char* dst, int dst_sz, unsigned int v) {
    char tmp[16];
    int n = 0;
    int i;
    if (dst_sz <= 0) return -1;
    if (v == 0) {
        int l = (int)strlen(dst);
        if (l + 1 >= dst_sz) return -1;
        dst[l] = '0';
        dst[l + 1] = '\0';
        return 0;
    }
    while (v > 0 && n < (int)sizeof(tmp)) {
        tmp[n++] = (char)('0' + (v % 10));
        v /= 10;
    }
    for (i = n - 1; i >= 0; --i) {
        int l = (int)strlen(dst);
        if (l + 1 >= dst_sz) return -1;
        dst[l] = tmp[i];
        dst[l + 1] = '\0';
    }
    return 0;
}

int main(void) {
    int master, slave;
    int pty_num = -1;
    int lock = 0;
    char slave_path[64];
    int ready_pipe[2];
    pid_t child;
    siginfo_t si;

    printf("=== pty/job-control test ===\n");

    master = open("/dev/ptmx", O_RDWR, 0);
    if (master < 0) {
        printf("[FAIL] open /dev/ptmx (errno=%d)\n", errno);
        return 1;
    }
    if (ioctl(master, TIOCGPTN, &pty_num) < 0 || pty_num < 0) {
        printf("[FAIL] ioctl(TIOCGPTN) (errno=%d)\n", errno);
        return 1;
    }
    if (ioctl(master, TIOCSPTLCK, &lock) < 0) {
        printf("[FAIL] ioctl(TIOCSPTLCK) (errno=%d)\n", errno);
        return 1;
    }

    strcpy(slave_path, "/dev/pts/");
    if (append_uint(slave_path, sizeof(slave_path), (unsigned int)pty_num) < 0) {
        printf("[FAIL] build slave path\n");
        return 1;
    }
    slave = open(slave_path, O_RDWR, 0);
    if (slave < 0) {
        printf("[FAIL] open slave %s (errno=%d)\n", slave_path, errno);
        return 1;
    }

    if (pipe(ready_pipe) < 0) {
        printf("[FAIL] pipe (errno=%d)\n", errno);
        return 1;
    }

    child = fork();
    if (child < 0) {
        printf("[FAIL] fork (errno=%d)\n", errno);
        return 1;
    }

    if (child == 0) {
        int child_slave;
        char ok = 'R';
        close(ready_pipe[0]);
        close(master);
        close(slave);

        if (setsid() < 0) exit(101);
        child_slave = open(slave_path, O_RDWR, 0);
        if (child_slave < 0) exit(102);
        if (ioctl(child_slave, TIOCSCTTY, &lock) < 0) exit(103);
        if (tcsetpgrp(child_slave, getpgrp()) < 0) exit(104);
        if (write(ready_pipe[1], &ok, 1) != 1) exit(105);

        for (;;) {
            char c;
            ssize_t n = read(child_slave, &c, 1);
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) continue;
        }
    }

    close(ready_pipe[1]);
    {
        char b = 0;
        if (read(ready_pipe[0], &b, 1) != 1 || b != 'R') {
            printf("[FAIL] child ready handshake (errno=%d)\n", errno);
            return 1;
        }
    }
    close(ready_pipe[0]);

    {
        unsigned char ctrl_z = 0x1A;
        if (write(master, &ctrl_z, 1) != 1) {
            printf("[FAIL] write Ctrl-Z to master (errno=%d)\n", errno);
            return 1;
        }
    }
    memset(&si, 0, sizeof(si));
    if (waitid(P_PID, child, &si, WSTOPPED) < 0) {
        printf("[FAIL] waitid WSTOPPED (errno=%d)\n", errno);
        return 1;
    }
    if (si.si_code != CLD_STOPPED) {
        printf("[FAIL] expected CLD_STOPPED got %d\n", si.si_code);
        return 1;
    }
    printf("[OK] pty generated stop\n");

    if (kill(child, SIGCONT) < 0) {
        printf("[FAIL] kill SIGCONT (errno=%d)\n", errno);
        return 1;
    }
    memset(&si, 0, sizeof(si));
    if (waitid(P_PID, child, &si, WCONTINUED) < 0) {
        printf("[FAIL] waitid WCONTINUED (errno=%d)\n", errno);
        return 1;
    }
    if (si.si_code != CLD_CONTINUED) {
        printf("[FAIL] expected CLD_CONTINUED got %d\n", si.si_code);
        return 1;
    }
    printf("[OK] continue event\n");

    {
        unsigned char ctrl_c = 0x03;
        if (write(master, &ctrl_c, 1) != 1) {
            printf("[FAIL] write Ctrl-C to master (errno=%d)\n", errno);
            return 1;
        }
    }
    memset(&si, 0, sizeof(si));
    if (waitid(P_PID, child, &si, WEXITED) < 0) {
        printf("[FAIL] waitid WEXITED (errno=%d)\n", errno);
        return 1;
    }
    if (si.si_code != CLD_KILLED || si.si_status != SIGINT) {
        printf("[FAIL] expected SIGINT death got code=%d status=%d\n", si.si_code, si.si_status);
        return 1;
    }
    printf("[OK] pty generated SIGINT termination\n");

    close(master);
    close(slave);
    printf("RESULT: PASS\n");
    return 0;
}
