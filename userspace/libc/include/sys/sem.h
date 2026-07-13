#ifndef _SYS_SEM_H
#define _SYS_SEM_H

#include <sys/ipc.h>

#define SEM_UNDO 0x1000

struct sembuf {
    unsigned short sem_num;
    short sem_op;
    short sem_flg;
};

int semget(key_t key, int nsems, int semflg);
int semop(int semid, struct sembuf *sops, size_t nsops);
int semctl(int semid, int semnum, int cmd, ...);

#endif
