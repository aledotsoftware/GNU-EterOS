#ifndef ETEROS_LINUX_COMPAT_H
#define ETEROS_LINUX_COMPAT_H

#include <types.h>

/* arch_prctl codes */
#define ARCH_SET_GS 0x1001
#define ARCH_SET_FS 0x1002
#define ARCH_GET_FS 0x1003
#define ARCH_GET_GS 0x1004

/* prctl codes */
#define PR_SET_NAME 15
#define PR_GET_NAME 16
#define PR_SET_VMA  0x53564d41
#define PR_SET_VMA_ANON_NAME 0

/* Ashmem IOCTLs and definitions */
#define ASHMEM_NAME_LEN 256
#define ASHMEM_SET_NAME 0x41007701
#define ASHMEM_GET_NAME 0x81007702
#define ASHMEM_SET_SIZE 0x40087703
#define ASHMEM_GET_SIZE 0x7704
#define ASHMEM_SET_PROT_MASK 0x40087705
#define ASHMEM_GET_PROT_MASK 0x7706
#define ASHMEM_ISPINNED 0x7707
#define ASHMEM_PIN 0x40087708
#define ASHMEM_UNPIN 0x40087709

struct ashmem_pin {
    uint32_t offset;
    uint32_t len;
};

/* Binder IOCTLs and definitions */
#define BINDER_VERSION_IOWR (int)0xc0046209
#define BINDER_WRITE_READ (int)0xc0306201
#define BINDER_SET_CONTEXT_MGR (int)0x40046207

struct binder_version {
    int32_t protocol_version;
};

struct binder_write_read {
    uint64_t write_size;
    uint64_t write_consumed;
    uint64_t write_buffer;
    uint64_t read_size;
    uint64_t read_consumed;
    uint64_t read_buffer;
};

struct binder_transaction_data {
    union {
        uint32_t handle;
        void *ptr;
    } target;
    void *cookie;
    uint32_t code;
    uint32_t flags;
    int32_t sender_pid;
    int32_t sender_euid;
    uint64_t data_size;
    uint64_t offsets_size;
    union {
        struct {
            uint64_t buffer;
            uint64_t offsets;
        } ptr;
        uint8_t buf[8];
    } data;
};

enum {
    BC_TRANSACTION = 0x40406300,
    BC_REPLY       = 0x40406301,
    BC_FREE_BUFFER = 0x40406303,
};

enum {
    BR_ERROR = 0x80047200,
    BR_OK    = 0x7201,
    BR_TRANSACTION = 0x80287202,
    BR_REPLY       = 0x80287203,
    BR_DEAD_REPLY  = 0x7205,
    BR_TRANSACTION_COMPLETE = 0x7206,
    BR_NOOP        = 0x720c,
};

/* Linux x86_64 stat structure for ABI compatibility */
struct linux_stat {
    uint64_t st_dev;
    uint64_t st_ino;
    uint64_t st_nlink;
    uint32_t st_mode;
    uint32_t st_uid;
    uint32_t st_gid;
    uint32_t __pad0;
    uint64_t st_rdev;
    int64_t  st_size;
    int64_t  st_blksize;
    int64_t  st_blocks;
    uint64_t st_atime;
    uint64_t st_atime_nsec;
    uint64_t st_mtime;
    uint64_t st_mtime_nsec;
    uint64_t st_ctime;
    uint64_t st_ctime_nsec;
    int64_t  __unused[3];
};

#endif /* ETEROS_LINUX_COMPAT_H */
