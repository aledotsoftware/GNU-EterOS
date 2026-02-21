#ifndef _SIGNAL_H
#define _SIGNAL_H

#include <stdint.h>

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
#define SA_RESTORER 0x04000000

typedef uint64_t sigset_t;
typedef void (*sighandler_t)(int);

struct sigaction {
    sighandler_t sa_handler;
    uint64_t     sa_flags;
    void       (*sa_restorer)(void);
    sigset_t     sa_mask;
};

/* Functions */
sighandler_t signal(int sig, sighandler_t handler);
int sigaction(int sig, const struct sigaction *act, struct sigaction *oldact);
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
int raise(int sig);

#endif /* _SIGNAL_H */
