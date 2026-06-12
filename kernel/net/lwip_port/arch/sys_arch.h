#ifndef LWIP_ARCH_SYS_ARCH_H
#define LWIP_ARCH_SYS_ARCH_H

#define SYS_MBOX_NULL (sys_mbox_t)NULL
#define SYS_SEM_NULL  (sys_sem_t)NULL

typedef void* sys_sem_t;
typedef void* sys_mutex_t;
typedef void* sys_mbox_t;
typedef int sys_thread_t;

#endif
