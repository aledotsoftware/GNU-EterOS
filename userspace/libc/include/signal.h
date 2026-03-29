#ifndef _SIGNAL_H
#define _SIGNAL_H

#include <stdint.h>
#include <stddef.h>

/* Standard Signals (Linux-compatible) */
#define SIGHUP      1
#define SIGINT      2
#define SIGQUIT     3
#define SIGILL      4
#define SIGTRAP     5
#define SIGABRT     6
#define SIGBUS      7
#define SIGFPE      8
#define SIGKILL     9
#define SIGUSR1     10
#define SIGSEGV     11
#define SIGUSR2     12
#define SIGPIPE     13
#define SIGALRM     14
#define SIGTERM     15
#define SIGCHLD     17
#define SIGCONT     18
#define SIGSTOP     19
#define SIGTSTP     20

/* Signal dispositions */
#define SIG_DFL     ((void (*)(int))0)
#define SIG_IGN     ((void (*)(int))1)
#define SIG_ERR     ((void (*)(int))-1)

/* sigprocmask how values */
#define SIG_BLOCK   0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

/* Signal flags */
#define SA_NOCLDSTOP 0x00000001
#define SA_NOCLDWAIT 0x00000002
#define SA_SIGINFO   0x00000004
#define SA_RESETHAND 0x80000000
#define SA_NODEFER   0x40000000
#define SA_ONSTACK  0x08000000
#define SA_RESTORER 0x04000000

typedef uint64_t sigset_t;
typedef void (*sighandler_t)(int);

typedef struct {
    void*  ss_sp;
    int    ss_flags;
    size_t ss_size;
} stack_t;

#define SS_ONSTACK  1
#define SS_DISABLE  2

struct sigaction {
    sighandler_t sa_handler;
    uint64_t     sa_flags;
    void       (*sa_restorer)(void);
    sigset_t     sa_mask;
};

/* Functions */
sighandler_t signal(int sig, sighandler_t handler);
int sigaction(int sig, const struct sigaction *act, struct sigaction *oldact);
int sigemptyset(sigset_t *set);
int sigfillset(sigset_t *set);
int sigaddset(sigset_t *set, int signum);
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
int sigpending(sigset_t *set);
int sigsuspend(const sigset_t *mask);
int sigaltstack(const stack_t *ss, stack_t *old_ss);
int raise(int sig);

#endif /* _SIGNAL_H */
