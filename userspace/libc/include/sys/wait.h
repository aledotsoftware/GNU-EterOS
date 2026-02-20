#ifndef _SYS_WAIT_H
#define _SYS_WAIT_H

#include <stdint.h>

typedef int pid_t;

pid_t wait(int *status);
pid_t waitpid(pid_t pid, int *status, int options);

/* Macros for interpreting status */
#define WIFEXITED(s)    (((s) & 0x7f) == 0)
#define WEXITSTATUS(s)  (((s) & 0xff00) >> 8)
#define WIFSIGNALED(s)  (((s) & 0x7f) > 0 && ((s) & 0x7f) < 0x7f)
#define WTERMSIG(s)     ((s) & 0x7f)

#endif /* _SYS_WAIT_H */
