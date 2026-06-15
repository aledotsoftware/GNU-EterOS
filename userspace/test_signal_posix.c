#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

static volatile int g_usr1_basic = 0;
static volatile int g_usr2_once = 0;
static volatile int g_nodefer_hits = 0;
static volatile int g_nodefer_depth = 0;
static volatile int g_nodefer_max_depth = 0;

static void on_usr1_basic(int sig) {
    (void)sig;
    g_usr1_basic++;
}

static void on_usr2_once(int sig) {
    (void)sig;
    g_usr2_once++;
}

static void on_usr1_nodefer(int sig) {
    (void)sig;
    g_nodefer_depth++;
    if (g_nodefer_depth > g_nodefer_max_depth) {
        g_nodefer_max_depth = g_nodefer_depth;
    }
    g_nodefer_hits++;
    if (g_nodefer_hits == 1) {
        (void)raise(SIGUSR1);
    }
    g_nodefer_depth--;
}

int main(void) {
    struct sigaction sa;
    struct sigaction old_sa;
    sigset_t set;

    printf("=== signal_posix test ===\n");

    /* 1) SIG_BLOCK/SIG_UNBLOCK with pending delivery */
    if (sigemptyset(&set) < 0 || sigaddset(&set, SIGUSR1) < 0) {
        printf("[FAIL] sigset init (errno=%d)\n", errno);
        return 1;
    }

    sa.sa_handler = on_usr1_basic;
    sa.sa_flags = 0;
    sa.sa_restorer = 0;
    sa.sa_mask = 0;
    if (sigaction(SIGUSR1, &sa, 0) < 0) {
        printf("[FAIL] sigaction(SIGUSR1 basic) (errno=%d)\n", errno);
        return 1;
    }

    if (sigprocmask(SIG_BLOCK, &set, 0) < 0) {
        printf("[FAIL] sigprocmask(SIG_BLOCK) (errno=%d)\n", errno);
        return 1;
    }
    if (raise(SIGUSR1) < 0) {
        printf("[FAIL] raise(SIGUSR1 blocked) (errno=%d)\n", errno);
        return 1;
    }
    if (g_usr1_basic != 0) {
        printf("[FAIL] blocked signal delivered too early\n");
        return 1;
    }

    if (sigprocmask(SIG_UNBLOCK, &set, 0) < 0) {
        printf("[FAIL] sigprocmask(SIG_UNBLOCK) (errno=%d)\n", errno);
        return 1;
    }
    (void)getpid(); /* Trigger a syscall boundary for deferred delivery. */
    if (g_usr1_basic == 0) {
        printf("[FAIL] pending signal not delivered after unblock\n");
        return 1;
    }
    printf("[OK] sigprocmask pending/unblock\n");

    /* 1b) sigpending + sigsuspend behavior */
    g_usr1_basic = 0;
    if (sigprocmask(SIG_BLOCK, &set, 0) < 0) {
        printf("[FAIL] sigprocmask(SIG_BLOCK #2) (errno=%d)\n", errno);
        return 1;
    }
    if (raise(SIGUSR1) < 0) {
        printf("[FAIL] raise(SIGUSR1 blocked #2) (errno=%d)\n", errno);
        return 1;
    }
    {
        sigset_t pending = 0;
        sigset_t suspend_mask = 0;
        if (sigpending(&pending) < 0) {
            printf("[FAIL] sigpending (errno=%d)\n", errno);
            return 1;
        }
        if ((pending & (1ULL << (SIGUSR1 - 1))) == 0) {
            printf("[FAIL] sigpending did not report SIGUSR1\n");
            return 1;
        }
        if (sigsuspend(&suspend_mask) != -1 || errno != EINTR) {
            printf("[FAIL] sigsuspend did not return EINTR (errno=%d)\n", errno);
            return 1;
        }
        (void)getpid();
        if (g_usr1_basic == 0) {
            printf("[FAIL] sigsuspend path did not deliver signal\n");
            return 1;
        }
    }
    printf("[OK] sigpending + sigsuspend\n");

    /* 2) SA_RESETHAND resets disposition after first handler run */
    g_usr2_once = 0;
    sa.sa_handler = on_usr2_once;
    sa.sa_flags = SA_RESETHAND;
    sa.sa_restorer = 0;
    sa.sa_mask = 0;
    if (sigaction(SIGUSR2, &sa, 0) < 0) {
        printf("[FAIL] sigaction(SIGUSR2 SA_RESETHAND) (errno=%d)\n", errno);
        return 1;
    }
    if (raise(SIGUSR2) < 0) {
        printf("[FAIL] raise(SIGUSR2) (errno=%d)\n", errno);
        return 1;
    }
    if (g_usr2_once != 1) {
        printf("[FAIL] SIGUSR2 handler did not run once\n");
        return 1;
    }
    if (sigaction(SIGUSR2, 0, &old_sa) < 0) {
        printf("[FAIL] sigaction(SIGUSR2 readback) (errno=%d)\n", errno);
        return 1;
    }
    if (old_sa.sa_handler != SIG_DFL) {
        printf("[FAIL] SA_RESETHAND did not restore SIG_DFL\n");
        return 1;
    }
    printf("[OK] SA_RESETHAND\n");

    /* 3) SA_NODEFER allows nested delivery of same signal */
    g_nodefer_hits = 0;
    g_nodefer_depth = 0;
    g_nodefer_max_depth = 0;
    sa.sa_handler = on_usr1_nodefer;
    sa.sa_flags = SA_NODEFER;
    sa.sa_restorer = 0;
    sa.sa_mask = 0;
    if (sigaction(SIGUSR1, &sa, 0) < 0) {
        printf("[FAIL] sigaction(SIGUSR1 SA_NODEFER) (errno=%d)\n", errno);
        return 1;
    }
    if (raise(SIGUSR1) < 0) {
        printf("[FAIL] raise(SIGUSR1 nodefer) (errno=%d)\n", errno);
        return 1;
    }
    if (g_nodefer_hits < 2 || g_nodefer_max_depth < 2) {
        printf("[FAIL] SA_NODEFER nesting failed (hits=%d depth=%d)\n",
               g_nodefer_hits, g_nodefer_max_depth);
        return 1;
    }
    printf("[OK] SA_NODEFER nesting\n");

    printf("RESULT: PASS\n");
    return 0;
}
