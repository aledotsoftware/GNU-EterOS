#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static int fail(const char* msg) {
    printf("[FAIL] %s (errno=%d)\n", msg, errno);
    return 1;
}

int main(void) {
    pid_t pid;
    siginfo_t si;

    printf("=== waitid/events test ===\n");

    pid = fork();
    if (pid < 0) {
        return fail("fork");
    }

    if (pid == 0) {
        for (;;) {
            sleep(1);
        }
    }

    sleep(1);

    if (kill(pid, SIGSTOP) < 0) return fail("kill(SIGSTOP)");
    memset(&si, 0, sizeof(si));
    if (waitid(P_PID, pid, &si, WSTOPPED) < 0) return fail("waitid(WSTOPPED)");
    if (si.si_pid != pid || si.si_code != CLD_STOPPED || si.si_status != SIGSTOP) {
        printf("[FAIL] unexpected stop event: pid=%d code=%d status=%d\n", si.si_pid, si.si_code, si.si_status);
        return 1;
    }
    printf("[OK] stop event\n");

    if (kill(pid, SIGCONT) < 0) return fail("kill(SIGCONT)");
    memset(&si, 0, sizeof(si));
    if (waitid(P_PID, pid, &si, WCONTINUED) < 0) return fail("waitid(WCONTINUED)");
    if (si.si_pid != pid || si.si_code != CLD_CONTINUED || si.si_status != SIGCONT) {
        printf("[FAIL] unexpected continue event: pid=%d code=%d status=%d\n", si.si_pid, si.si_code, si.si_status);
        return 1;
    }
    printf("[OK] continue event\n");

    if (kill(pid, SIGKILL) < 0) return fail("kill(SIGKILL)");
    memset(&si, 0, sizeof(si));
    if (waitid(P_PID, pid, &si, WEXITED) < 0) return fail("waitid(WEXITED)");
    if (si.si_pid != pid || si.si_code != CLD_KILLED || si.si_status != SIGKILL) {
        printf("[FAIL] unexpected kill event: pid=%d code=%d status=%d\n", si.si_pid, si.si_code, si.si_status);
        return 1;
    }
    printf("[OK] killed event\n");

    printf("RESULT: PASS\n");
    return 0;
}
