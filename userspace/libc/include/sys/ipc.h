#ifndef _SYS_IPC_H
#define _SYS_IPC_H

#include <sys/types.h>

#define IPC_CREAT  0001000
#define IPC_EXCL   0002000
#define IPC_NOWAIT 0004000

#define IPC_RMID 0
#define IPC_SET  1
#define IPC_STAT 2
#define IPC_INFO 3

struct ipc_perm {
    key_t key;
    uid_t uid;
    gid_t gid;
    uid_t cuid;
    gid_t cgid;
    mode_t mode;
    unsigned short __seq;
};

#endif
