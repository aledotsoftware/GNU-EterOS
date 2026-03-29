#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>

static volatile int got_sig = 0;
static volatile int on_alt = 0;
static volatile unsigned long alt_lo = 0;
static volatile unsigned long alt_hi = 0;

static void on_usr1(int sig) {
    volatile char marker = 0;
    unsigned long p = (unsigned long)&marker;
    (void)sig;
    got_sig = 1;
    if (p >= alt_lo && p < alt_hi) {
        on_alt = 1;
    }
}

int main(void) {
    stack_t ss;
    stack_t oldss;
    struct sigaction sa;
    char* mem;

    printf("=== sigaltstack test ===\n");

    mem = (char*)malloc(16384);
    if (!mem) {
        printf("[FAIL] malloc alt stack\n");
        return 1;
    }

    alt_lo = (unsigned long)mem;
    alt_hi = (unsigned long)(mem + 16384);

    ss.ss_sp = mem;
    ss.ss_size = 16384;
    ss.ss_flags = 0;
    if (sigaltstack(&ss, &oldss) < 0) {
        printf("[FAIL] sigaltstack set (errno=%d)\n", errno);
        return 1;
    }
    printf("[OK] sigaltstack set\n");

    sa.sa_handler = on_usr1;
    sa.sa_flags = SA_ONSTACK;
    sa.sa_mask = 0;
    sa.sa_restorer = 0;
    if (sigaction(SIGUSR1, &sa, 0) < 0) {
        printf("[FAIL] sigaction SA_ONSTACK (errno=%d)\n", errno);
        return 1;
    }
    printf("[OK] sigaction SA_ONSTACK\n");

    if (raise(SIGUSR1) < 0) {
        printf("[FAIL] raise(SIGUSR1) (errno=%d)\n", errno);
        return 1;
    }

    if (!got_sig) {
        printf("[FAIL] signal not delivered\n");
        return 1;
    }
    if (!on_alt) {
        printf("[FAIL] handler did not run on alt stack\n");
        return 1;
    }
    printf("[OK] handler ran on alt stack\n");
    printf("RESULT: PASS\n");
    return 0;
}
