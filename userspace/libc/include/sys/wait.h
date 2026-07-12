#ifndef _SYS_WAIT_H
#define _SYS_WAIT_H

#include <stdint.h>

typedef int pid_t;
typedef int idtype_t;

typedef struct siginfo {
    int si_signo;
    int si_errno;
    int si_code;
    int si_pid;
    int si_uid;
    int si_status;
} siginfo_t;

#define P_ALL  0
#define P_PID  1
#define P_PGID 2

#define WNOHANG     1
#define WUNTRACED   2
#ifndef WSTOPPED
#define WSTOPPED    WUNTRACED
#endif
#define WEXITED     4
#define WCONTINUED  8
#define WNOWAIT     0x01000000
#define __WALL      0x40000000

#define CLD_EXITED      1
#define CLD_KILLED      2
#define CLD_DUMPED      3
#define CLD_TRAPPED     4
#define CLD_STOPPED     5
#define CLD_CONTINUED   6

pid_t wait(int *status);
pid_t waitpid(pid_t pid, int *status, int options);
int waitid(idtype_t idtype, pid_t id, siginfo_t *infop, int options);

/* Macros for interpreting status */
#ifndef WIFEXITED
#define WIFEXITED(s)    (((s) & 0x7f) == 0)
#endif
#ifndef WEXITSTATUS
#define WEXITSTATUS(s)  (((s) & 0xff00) >> 8)
#endif
#ifndef WIFSIGNALED
#define WIFSIGNALED(s)  (((s) & 0x7f) > 0 && ((s) & 0x7f) < 0x7f)
#endif
#ifndef WTERMSIG
#define WTERMSIG(s)     ((s) & 0x7f)
#endif
#ifndef WIFSTOPPED
#define WIFSTOPPED(s)   (((s) & 0xff) == 0x7f)
#endif
#ifndef WSTOPSIG
#define WSTOPSIG(s)     (((s) >> 8) & 0xff)
#endif
#ifndef WIFCONTINUED
#define WIFCONTINUED(s) ((s) == 0xffff)
#endif

#endif /* _SYS_WAIT_H */
