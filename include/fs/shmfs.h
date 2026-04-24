#ifndef FS_SHMFS_H
#define FS_SHMFS_H

#include <types.h>
#include <lock.h>

/* Defines a shared memory object, holding a list of allocated pages */
typedef struct shm_object {
    char name[128];
    uint32_t size;            /* Size in bytes */
    uint32_t page_count;      /* Number of allocated physical pages */
    uint64_t* pages;          /* Array of physical page addresses */
    uint32_t ref_count;       /* Number of open file descriptors to this object */
    spinlock_t lock;
    struct shm_object* next;
} shm_object_t;

struct fs_node* shmfs_init(void);

/* Creates an anonymous shared memory node (for memfd_create) */
struct fs_node* shmfs_create_memfd(const char* name);

#endif /* FS_SHMFS_H */
