#ifndef ETEROS_SCHED_H
#define ETEROS_SCHED_H

/* Linux clone() flags */
#define CLONE_VM        0x00000100 /* set if VM shared between processes */
#define CLONE_FS        0x00000200 /* set if fs info shared between processes */
#define CLONE_FILES     0x00000400 /* set if open files shared between processes */
#define CLONE_SIGHAND   0x00000800 /* set if signal handlers and blocked signals shared */
#define CLONE_THREAD    0x00010000 /* Same thread group? */
#define CLONE_SYSVSEM   0x00040000 /* share system V SEM_UNDO semantics */
#define CLONE_SETTLS    0x00080000 /* create a new TLS for the child */
#define CLONE_PARENT_SETTID 0x00100000 /* set the TID in the parent */
#define CLONE_CHILD_CLEARTID 0x00200000 /* clear the TID in the child */
#define CLONE_CHILD_SETTID 0x01000000 /* set the TID in the child */

/* sys_wait4 options */
#define WNOHANG     1
#define WUNTRACED   2
#define WSTOPPED    WUNTRACED
#define WEXITED     4
#define WCONTINUED  8
#define WNOWAIT     0x01000000
#define __WNOTHREAD 0x20000000
#define __WALL      0x40000000
#define __WCLONE    0x80000000

/* waitid idtypes */
#define P_ALL       0
#define P_PID       1
#define P_PGID      2

/* waitid si_code values for SIGCHLD */
#define CLD_EXITED      1
#define CLD_KILLED      2
#define CLD_DUMPED      3
#define CLD_TRAPPED     4
#define CLD_STOPPED     5
#define CLD_CONTINUED   6

#endif /* ETEROS_SCHED_H */
