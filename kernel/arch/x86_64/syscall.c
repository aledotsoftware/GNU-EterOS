/**
 * éterOS - x86_64 Syscall Handler
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 */

#include <syscall.h>
#include <elf.h>
#include <msr.h>
#include <cpu.h>
#include <serial.h>
#include <vga.h>
#include <string.h>
#include <task.h>
#include <keyboard.h>
#include <fs/vfs.h>
#include <mm.h>
#include <pmm.h>
#include <hal.h>
#include <errno.h>
#include <sys/signal.h>
#include <lock.h>
#include <timer.h>
#include <vmm.h>
#include <futex.h>
#include <net/socket.h>
#include <net/defs.h>
#include <fcntl.h>
#include <ioctl.h>
#include <termios.h>

#ifndef offsetof
#define offsetof(type, member) ((size_t) &((type *)0)->member)
#endif

extern void syscall_entry(void);

/* External Declarations for Task Functions */
extern int task_exec(const char* path, char* const argv[], char* const envp[], struct syscall_regs* regs);
extern int task_waitpid(int pid, int* status, int options);

/* Structures used in syscalls */
struct timespec {
    int64_t tv_sec;
    int64_t tv_nsec;
};

void syscall_init(void) {
    serial_write_string("[SYSCALL] Initializing MSRs for x86_64...\n");

    /* 1. EFER: Enable System Call Extensions (SCE) */
    uint64_t efer = rdmsr(MSR_EFER);
    efer |= EFER_SCE;
    wrmsr(MSR_EFER, efer);

    /* 2. STAR: Syscall/Sysret target CS/SS */
    uint64_t star = 0;
    star |= (uint64_t)0x0008 << 32; /* Kernel CS Base */
    star |= (uint64_t)0x0010 << 48; /* User CS Base */
    wrmsr(MSR_STAR, star);

    /* 3. LSTAR: Target RIP for syscall */
    wrmsr(MSR_LSTAR, (uint64_t)syscall_entry);

    /* 4. SFMASK: RFLAGS mask (clear IF 0x200, TF 0x100) */
    wrmsr(MSR_SFMASK, 0x200);

    serial_write_string("[SYSCALL] x86_64 mechanism enabled.\n");
}

/* ========================================================================= */
/* Syscall Implementations                                                   */
/* ========================================================================= */

/* --- Socket VFS Wrappers --- */

static ssize_t socket_read_fs(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    (void)offset;
    if ((node->flags & 0x7) != FS_SOCKET) return 0;
    int res = net_recv((int)node->inode, buffer, size, 0);
    return (res < 0) ? 0 : (ssize_t)res;
}

static uint32_t socket_write_fs(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    (void)offset;
    if ((node->flags & 0x7) != FS_SOCKET) return 0;
    int res = net_send((int)node->inode, buffer, size, 0);
    return (res < 0) ? 0 : (uint32_t)res;
}

static void socket_close_fs(fs_node_t* node) {
    if ((node->flags & 0x7) == FS_SOCKET) {
        net_close((int)node->inode);
    }
}

/* --- End Socket VFS Wrappers --- */

#define UIO_MAXIOV  1024

typedef struct {
    uint8_t* buffer;
    uint32_t size;
    uint32_t read_ptr;
    uint32_t write_ptr;
    uint32_t bytes_available;
    int readers;
    int writers;
    spinlock_t lock;
} pipe_t;

#define PIPE_SIZE 4096
#define PIPE_READ_END  1
#define PIPE_WRITE_END 2

static ssize_t pipe_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    (void)offset;
    pipe_t* pipe = (pipe_t*)node->ptr;
    if (!pipe) return 0;

    ssize_t read = 0;
    while (read < size) {
        spin_lock(&pipe->lock);

        if (pipe->bytes_available == 0) {
            spin_unlock(&pipe->lock);
            if (pipe->writers == 0) return read; /* EOF */
            task_yield(); /* Block */
            continue;
        }

        while (read < size && pipe->bytes_available > 0) {
            buffer[read++] = pipe->buffer[pipe->read_ptr];
            pipe->read_ptr = (pipe->read_ptr + 1) % pipe->size;
            pipe->bytes_available--;
        }

        spin_unlock(&pipe->lock);
    }
    return read;
}

static uint32_t pipe_write(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    (void)offset;
    pipe_t* pipe = (pipe_t*)node->ptr;
    if (!pipe) return 0;

    uint32_t written = 0;
    while (written < size) {
        spin_lock(&pipe->lock);

        if (pipe->bytes_available == pipe->size) {
            spin_unlock(&pipe->lock);
            if (pipe->readers == 0) return written; /* Broken pipe */
            task_yield(); /* Block */
            continue;
        }

        while (written < size && pipe->bytes_available < pipe->size) {
            pipe->buffer[pipe->write_ptr] = buffer[written++];
            pipe->write_ptr = (pipe->write_ptr + 1) % pipe->size;
            pipe->bytes_available++;
        }

        spin_unlock(&pipe->lock);
    }
    return written;
}

static void pipe_close(fs_node_t* node) {
    pipe_t* pipe = (pipe_t*)node->ptr;
    if (!pipe) return;

    spin_lock(&pipe->lock);

    if (node->impl == PIPE_READ_END) {
        pipe->readers--;
    } else if (node->impl == PIPE_WRITE_END) {
        pipe->writers--;
    } else {
        /* Fallback for legacy or unknown nodes (shouldn't happen with new pipes) */
        if (node->write == 0) pipe->readers--;
        else pipe->writers--;
    }

    if (pipe->readers <= 0 && pipe->writers <= 0) {
        kfree(pipe->buffer);
        kfree(pipe);
    } else {
        spin_unlock(&pipe->lock);
    }
}

static void pipe_open(fs_node_t* node) {
    (void)node;
}

static int split_path(const char* path, char* parent, char* name) {
    int len = strlen(path);
    int last_slash = -1;
    for (int i = 0; i < len; i++) {
        if (path[i] == '/') last_slash = i;
    }

    if (last_slash == -1) {
        strlcpy(parent, "/", 128);
        strlcpy(name, path, 128);
    } else {
        if (last_slash == 0) {
            strlcpy(parent, "/", 128);
        } else {
            if (last_slash >= 128) return -1;
            memcpy(parent, path, last_slash);
            parent[last_slash] = 0;
        }
        strlcpy(name, path + last_slash + 1, 128);
    }
    return 0;
}

static int copy_user_string(const char* user_path, char** out_kpath, int max_len) {
    if (!user_path) return -EFAULT;
    char* kpath = (char*)kmalloc(max_len);
    if (!kpath) return -ENOMEM;
    int res = vmm_strncpy_from_user(kpath, user_path, max_len);
    if (res < 0) {
        kfree(kpath);
        return res;
    }
    kpath[max_len - 1] = '\0'; /* Ensure null termination */
    *out_kpath = kpath;
    return 0;
}

static int64_t sys_mmap(void* addr, size_t len, int prot, int flags, int fd, int64_t offset) {
    (void)fd; (void)offset;
    if (len == 0) return -EINVAL;
    if (!(flags & 0x20)) return -ENODEV;

    task_t* current = task_get_current();
    uint64_t virt;

    if (addr && (flags & 0x10)) {
        virt = (uint64_t)addr;
        if (virt & 0xFFF) return -EINVAL;
        if (!vmm_validate_user_ptr(addr, len)) return -ENOMEM;
    } else {
        virt = current->mmap_base;
        uint64_t aligned_len = PAGE_ALIGN_UP(len);
        if (aligned_len < len) return -ENOMEM; /* Overflow in alignment */

        uint64_t new_base = virt + aligned_len;
        if (new_base < virt || new_base > (USER_LIMIT + 1)) return -ENOMEM;

        current->mmap_base = new_base;
    }

    uint64_t start = PAGE_ALIGN_DOWN(virt);
    uint64_t end = PAGE_ALIGN_UP(virt + len);
    if (end < start || (virt + len) < virt) return -ENOMEM; /* Overflow check */

    for (uint64_t v = start; v < end; v += PAGE_SIZE) {
        /* SECURITY FIX: Check if page is already mapped. If so, unmap it to prevent data leaks or reuse. */
        uint64_t existing_phys = hal_mem_get_phys(v);
        if (existing_phys != 0) {
            pmm_unref_page((void*)existing_phys);
            vmm_unmap_page(v);
        }

        if (hal_mem_get_phys(v) == 0) {
            void* phys = pmm_alloc_page();
            if (!phys) return -ENOMEM;
            uint32_t map_flags = HAL_MEM_USER | HAL_MEM_READ;
            if (prot & 2) map_flags |= HAL_MEM_WRITE;
            if (prot & 4) map_flags |= HAL_MEM_EXEC;
            hal_mem_map((uint64_t)phys, v, map_flags);
            memset((void*)v, 0, PAGE_SIZE);
        }
    }
    return virt;
}

static int64_t sys_socket(int domain, int type, int protocol) {
    if (domain != AF_INET) return -EAFNOSUPPORT;
    if (type != SOCK_STREAM) return -EPROTOTYPE;
    if (protocol != 0 && protocol != IPPROTO_TCP) return -EPROTONOSUPPORT;

    int sock_id = net_socket(domain, type, IPPROTO_TCP);
    if (sock_id < 0) return -ENOMEM;

    task_t* current = task_get_current();
    int fd = -1;
    for (int i = 3; i < MAX_FD; i++) {
        if (current->fd_table[i].node == NULL) {
            fd = i;
            break;
        }
    }
    if (fd == -1) {
        net_close(sock_id);
        return -EMFILE;
    }

    fs_node_t* node = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!node) {
        net_close(sock_id);
        return -ENOMEM;
    }
    memset(node, 0, sizeof(fs_node_t));
    strlcpy(node->name, "socket", 32);
    node->flags = FS_SOCKET;
    node->inode = (uint32_t)sock_id;
    node->read = socket_read_fs;
    node->write = socket_write_fs;
    node->close = socket_close_fs;
    node->ref_count = 1;

    current->fd_table[fd].node = node;
    current->fd_table[fd].offset = 0;
    current->fd_table[fd].flags = O_RDWR;
    return fd;
}

static int64_t sys_connect(int fd, const struct sockaddr* addr, int addrlen) {
    if (!vmm_verify_user_access(addr, addrlen, 0)) return -EFAULT;
    if (addrlen < (int)sizeof(struct sockaddr_in)) return -EINVAL;

    task_t* current = task_get_current();
    if (fd < 0 || fd >= MAX_FD) return -EBADF;
    if (!current->fd_table[fd].node) return -EBADF;

    fs_node_t* node = current->fd_table[fd].node;
    if ((node->flags & 0x7) != FS_SOCKET) return -ENOTSOCK;

    struct sockaddr_in* sin = (struct sockaddr_in*)addr;
    if (sin->sin_family != AF_INET) return -EAFNOSUPPORT;

    int res = net_connect((int)node->inode, sin, addrlen);
    if (res < 0) {
        if (res == -2) return -ENETUNREACH;
        return -ECONNREFUSED;
    }
    return 0;
}

static int64_t sys_pipe(int* pipefd) {
    if (!vmm_verify_user_access(pipefd, 2 * sizeof(int), 1)) return -EFAULT;
    task_t* current = task_get_current();
    int fd[2] = {-1, -1};
    int count = 0;
    for (int i = 3; i < MAX_FD && count < 2; i++) {
        if (current->fd_table[i].node == NULL) fd[count++] = i;
    }
    if (count < 2) return -EMFILE;

    pipe_t* pipe = (pipe_t*)kmalloc(sizeof(pipe_t));
    if (!pipe) return -ENOMEM;
    memset(pipe, 0, sizeof(pipe_t));
    pipe->buffer = (uint8_t*)kmalloc(PIPE_SIZE);
    if (!pipe->buffer) { kfree(pipe); return -ENOMEM; }
    pipe->size = PIPE_SIZE;
    pipe->readers = 1; pipe->writers = 1;

    fs_node_t* reader = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!reader) { kfree(pipe->buffer); kfree(pipe); return -ENOMEM; }
    fs_node_t* writer = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!writer) { kfree(reader); kfree(pipe->buffer); kfree(pipe); return -ENOMEM; }
    memset(reader, 0, sizeof(fs_node_t)); memset(writer, 0, sizeof(fs_node_t));
    reader->ref_count = 1; writer->ref_count = 1;
    strlcpy(reader->name, "pipe_r", 32); reader->flags = FS_PIPE; reader->read = pipe_read; reader->close = pipe_close; reader->open = pipe_open; reader->ptr = (struct fs_node*)pipe;
    reader->impl = PIPE_READ_END;

    strlcpy(writer->name, "pipe_w", 32); writer->flags = FS_PIPE; writer->write = pipe_write; writer->close = pipe_close; writer->open = pipe_open; writer->ptr = (struct fs_node*)pipe;
    writer->impl = PIPE_WRITE_END;

    current->fd_table[fd[0]].node = reader; current->fd_table[fd[0]].offset = 0; current->fd_table[fd[0]].flags = O_RDONLY;
    current->fd_table[fd[1]].node = writer; current->fd_table[fd[1]].offset = 0; current->fd_table[fd[1]].flags = O_WRONLY;
    pipefd[0] = fd[0]; pipefd[1] = fd[1];
    return 0;
}

#define ARCH_SET_GS 0x1001
#define ARCH_SET_FS 0x1002

struct utsname {
    char sysname[65]; char nodename[65]; char release[65]; char version[65]; char machine[65]; char domainname[65];
};

struct iovec { void  *iov_base; size_t iov_len; };

/* Helper to safely copy iovec array from user space */
static int copy_from_user_iovec(const struct iovec* user_iov, int iovcnt, struct iovec** out_kiov) {
    if (iovcnt < 0) return -EINVAL;
    if (iovcnt == 0) {
        *out_kiov = NULL;
        return 0;
    }
    if (!user_iov) return -EINVAL;
    size_t size = sizeof(struct iovec) * iovcnt;

    /* Verify user access rights for the array */
    if (!vmm_verify_user_access(user_iov, size, 0)) return -EFAULT;

    struct iovec* kiov = (struct iovec*)kmalloc(size);
    if (!kiov) return -ENOMEM;

    /* Copy from user space to kernel space */
    memcpy(kiov, user_iov, size);
    *out_kiov = kiov;
    return 0;
}

struct stat {
    uint64_t st_dev; uint64_t st_ino; uint64_t st_nlink; uint32_t st_mode; uint32_t st_uid; uint32_t st_gid; uint32_t __pad0; uint64_t st_rdev; int64_t  st_size; int64_t  st_blksize; int64_t  st_blocks; uint64_t st_atime; uint64_t st_atime_nsec; uint64_t st_mtime; uint64_t st_mtime_nsec; uint64_t st_ctime; uint64_t st_ctime_nsec; int64_t  __unused[3];
};

static int64_t sys_write(int fd, const void* buf, size_t count) {
    if (!vmm_verify_user_access(buf, count, 0)) return -EFAULT;
    task_t* current = task_get_current();
    if (fd < 0 || fd >= MAX_FD) return -EBADF;
    if (!current->fd_table[fd].node) return -EBADF;

    if ((current->fd_table[fd].flags & O_ACCMODE) == O_RDONLY) return -EBADF;

    uint32_t written = write_fs(current->fd_table[fd].node, current->fd_table[fd].offset, count, (uint8_t*)buf);
    current->fd_table[fd].offset += written;
    return written;
}

static int64_t sys_read(int fd, void* buf, size_t count) {
    if (!vmm_verify_user_access(buf, count, 1)) return -EFAULT;
    task_t* current = task_get_current();
    if (fd < 0 || fd >= MAX_FD) return -EBADF;
    if (!current->fd_table[fd].node) return -EBADF;

    if ((current->fd_table[fd].flags & O_ACCMODE) == O_WRONLY) return -EBADF;

    ssize_t read = read_fs(current->fd_table[fd].node, current->fd_table[fd].offset, count, (uint8_t*)buf);
    if (read > 0) {
        current->fd_table[fd].offset += read;
    }
    return read;
}

#define AT_FDCWD -100
#define AT_EMPTY_PATH 0x1000

static int64_t sys_openat(int dirfd, const char* path, int flags, int mode) {
    char* kpath = NULL;
    int res = copy_user_string(path, &kpath, 4096);
    if (res < 0) return res;

    task_t* current = task_get_current();
    int fd = -1;
    for (int i = 3; i < MAX_FD; i++) { if (current->fd_table[i].node == NULL) { fd = i; break; } }
    if (fd == -1) { kfree(kpath); return -EMFILE; }

    fs_node_t* base_node;
    if (kpath[0] == '/') {
        base_node = fs_root;
    } else if (dirfd == AT_FDCWD) {
        base_node = current->cwd_node;
    } else {
        if (dirfd < 0 || dirfd >= MAX_FD || !current->fd_table[dirfd].node) {
            kfree(kpath);
            return -EBADF;
        }
        base_node = current->fd_table[dirfd].node;
    }

    fs_node_t* node = vfs_lookup(base_node, kpath);
    if (!node) {
        if (flags & O_CREAT) {
            char parent_path[128]; char filename[128];
            if (split_path(kpath, parent_path, filename) != 0) { kfree(kpath); return -ENAMETOOLONG; }
            fs_node_t* parent = vfs_lookup(base_node, parent_path);
            if (!parent) { kfree(kpath); return -ENOENT; }
            if (create_fs(parent, filename, (uint16_t)mode) != 0) { kfree(parent); kfree(kpath); return -EACCES; }
            node = vfs_lookup(base_node, kpath);
            kfree(parent);
            if (!node) { kfree(kpath); return -ENOENT; }
        } else { kfree(kpath); return -ENOENT; }
    }
    kfree(kpath);

    uint8_t read_mode = 0; uint8_t write_mode = 0;
    if ((flags & O_ACCMODE) == O_RDONLY) { read_mode = 1; }
    else if ((flags & O_ACCMODE) == O_WRONLY) { write_mode = 1; }
    else if ((flags & O_ACCMODE) == O_RDWR) { read_mode = 1; write_mode = 1; }

    if (flags & O_NONBLOCK) node->flags |= O_NONBLOCK;

    /* Prevent opening directories for writing */
    if ((node->flags & 0x7) == FS_DIRECTORY && (write_mode != 0)) {
        kfree(node);
        return -EISDIR;
    }

    open_fs(node, read_mode, write_mode);
    current->fd_table[fd].node = node; current->fd_table[fd].offset = 0; current->fd_table[fd].flags = flags;
    return fd;
}

static int64_t sys_open(const char* path, int flags, int mode) {
    return sys_openat(AT_FDCWD, path, flags, mode);
}

static int64_t sys_newfstatat(int dirfd, const char* path, struct stat* buf, int flags) {
    char* kpath = NULL;
    int res = copy_user_string(path, &kpath, 4096);
    if (res < 0) return res;

    task_t* current = task_get_current();
    if (!vmm_verify_user_access(buf, sizeof(struct stat), 1)) { kfree(kpath); return -EFAULT; }

    fs_node_t* base_node;
    if (kpath[0] == '/') {
        base_node = fs_root;
    } else if (dirfd == AT_FDCWD) {
        base_node = current->cwd_node;
    } else {
        if (dirfd < 0 || dirfd >= MAX_FD || !current->fd_table[dirfd].node) {
            kfree(kpath);
            return -EBADF;
        }
        base_node = current->fd_table[dirfd].node;
    }

    fs_node_t* node = NULL;
    if ((flags & AT_EMPTY_PATH) && kpath[0] == '\0') {
        node = (fs_node_t*)kmalloc(sizeof(fs_node_t));
        if (node) {
            memcpy(node, base_node, sizeof(fs_node_t));
        }
    } else {
        node = vfs_lookup(base_node, kpath);
    }

    if (!node) { kfree(kpath); return -ENOENT; }
    memset(buf, 0, sizeof(struct stat));
    buf->st_ino = node->inode; buf->st_size = node->length; buf->st_mode = 0100644;
    if ((node->flags & 0x7) == FS_DIRECTORY) buf->st_mode = 0040755;
    kfree(node);
    kfree(kpath);
    return 0;
}

static int64_t sys_close(int fd) {
    task_t* current = task_get_current();
    if (fd < 0 || fd >= MAX_FD) return -EBADF;
    if (!current->fd_table[fd].node) return -EBADF;
    fs_node_t* node = current->fd_table[fd].node;
    if (__atomic_sub_fetch(&node->ref_count, 1, __ATOMIC_SEQ_CST) == 0) { close_fs(node); kfree(node); }
    current->fd_table[fd].node = NULL;
    return 0;
}

static int64_t sys_mkdir(const char* path, int mode) {
    char* kpath = NULL;
    int res = copy_user_string(path, &kpath, 4096);
    if (res < 0) return res;

    char parent_path[128]; char filename[128];
    if (split_path(kpath, parent_path, filename) != 0) { kfree(kpath); return -ENAMETOOLONG; }
    fs_node_t* parent = vfs_lookup(fs_root, parent_path);
    if (!parent) { kfree(kpath); return -ENOENT; }
    int res2 = mkdir_fs(parent, filename, (uint16_t)mode);
    kfree(parent);
    kfree(kpath);
    return res2;
}

static int64_t sys_unlink(const char* path) {
    char* kpath = NULL;
    int res = copy_user_string(path, &kpath, 4096);
    if (res < 0) return res;

    char parent_path[128]; char filename[128];
    if (split_path(kpath, parent_path, filename) != 0) { kfree(kpath); return -ENAMETOOLONG; }
    fs_node_t* parent = vfs_lookup(fs_root, parent_path);
    if (!parent) { kfree(kpath); return -ENOENT; }
    int res2 = unlink_fs(parent, filename);
    kfree(parent);
    kfree(kpath);
    return res2;
}

static int64_t sys_lseek(int fd, int64_t offset, int whence) {
    task_t* current = task_get_current();
    if (fd < 0 || fd >= MAX_FD) return -EBADF;
    if (!current->fd_table[fd].node) return -EBADF;
    if (whence == 0) current->fd_table[fd].offset = offset;
    else if (whence == 1) current->fd_table[fd].offset += offset;
    else if (whence == 2) current->fd_table[fd].offset = current->fd_table[fd].node->length + offset;
    else return -EINVAL;
    return current->fd_table[fd].offset;
}

static int64_t sys_getpid(void) { return task_get_current()->id; }

static int64_t sys_kill(int pid, int sig) {
    if (pid <= 0) return -EINVAL;
    if (sig < 0 || sig > 31) return -EINVAL;
    if (sig == 0) return task_get_by_id((uint32_t)pid) ? 0 : -ESRCH;
    if (sig == SIGKILL || sig == SIGTERM) {
        if (task_kill((uint32_t)pid) == 0) return 0;
        return -ESRCH;
    }
    task_t* target = task_get_by_id((uint32_t)pid);
    if (!target) return -ESRCH;
    if (target->state == TASK_DEAD) return -ESRCH;
    target->signal_pending |= (1u << sig);
    if (target->state == TASK_BLOCKED || target->state == TASK_SLEEPING) task_wakeup(target);
    return 0;
}

static int64_t sys_brk(uint64_t brk) {
    task_t* current = task_get_current();
    if (brk == 0) return current->brk;
    /* Check boundary */
    if (brk > USER_LIMIT) return current->brk;

    if (brk < current->brk) { current->brk = brk; return current->brk; }
    uint64_t old_page = PAGE_ALIGN_UP(current->brk);
    uint64_t new_page = PAGE_ALIGN_UP(brk);

    /* Check for integer overflow during page alignment */
    if (new_page < brk) return current->brk;
    if (new_page > old_page) {
        for (uint64_t addr = old_page; addr < new_page; addr += PAGE_SIZE) {
             if (hal_mem_get_phys(addr) == 0) {
                  void* phys = pmm_alloc_page();
                  if (!phys) return -ENOMEM;
                  hal_mem_map((uint64_t)phys, addr, HAL_MEM_READ | HAL_MEM_WRITE | HAL_MEM_USER);
                  memset((void*)addr, 0, PAGE_SIZE);
             }
        }
    }
    current->brk = brk;
    return current->brk;
}

static int64_t sys_arch_prctl(int code, uint64_t addr) {
    task_t* current = task_get_current();
    if (code == ARCH_SET_FS) { current->fs_base = addr; wrmsr(MSR_FS_BASE, addr); return 0; }
    else if (code == ARCH_SET_GS) { current->gs_base = addr; wrmsr(MSR_KERNEL_GS_BASE, addr); return 0; }
    return -EINVAL;
}

static int64_t sys_uname(struct utsname* buf) {
    if (!vmm_verify_user_access(buf, sizeof(struct utsname), 1)) return -EFAULT;
    strlcpy(buf->sysname, "Linux", 65); strlcpy(buf->nodename, "eterOS", 65);
    strlcpy(buf->release, "5.5.0-generic", 65); strlcpy(buf->version, "#1 SMP 2026", 65);
    strlcpy(buf->machine, "x86_64", 65); strlcpy(buf->domainname, "localdomain", 65);
    return 0;
}

static int64_t sys_ioctl(int fd, unsigned long request, void* arg) {
    /* SECURITY FIX: Validate pointers for known IOCTLs */
    /* Legacy IOCTLs (TCGETS/TCSETS) use void* arg as pointer without encoding */
    if (request == TCGETS) {
        if (!vmm_verify_user_access(arg, sizeof(struct termios), 1)) return -EFAULT;
    } else if (request == TCSETS) {
        if (!vmm_verify_user_access(arg, sizeof(struct termios), 0)) return -EFAULT;
    } else if (request == TIOCGWINSZ) {
        if (!vmm_verify_user_access(arg, sizeof(struct winsize), 1)) return -EFAULT;
    } else if (request == TIOCSWINSZ) {
        if (!vmm_verify_user_access(arg, sizeof(struct winsize), 0)) return -EFAULT;
    } else if (request == FIONBIO) {
        if (!vmm_verify_user_access(arg, sizeof(int), 0)) return -EFAULT;
    }

    task_t* current = task_get_current();
    if (fd < 0 || fd >= MAX_FD) return -EBADF;
    if (!current->fd_table[fd].node) return -EBADF;
    return ioctl_fs(current->fd_table[fd].node, (int)request, arg);
}

static int64_t sys_writev(int fd, const struct iovec *iov, int iovcnt) {
    if (fd < 0 || !iov) return -EINVAL;
    if (iovcnt < 0 || iovcnt > UIO_MAXIOV) return -EINVAL;

    struct iovec* kiov = NULL;
    int res = copy_from_user_iovec(iov, iovcnt, &kiov);
    if (res < 0) return res;

    int64_t total = 0;
    for (int i=0; i<iovcnt; i++) {
        int64_t r = sys_write(fd, kiov[i].iov_base, kiov[i].iov_len);
        if (r < 0) {
            kfree(kiov);
            return r;
        }
        total += r;
    }
    kfree(kiov);
    return total;
}

static int64_t sys_readv(int fd, const struct iovec *iov, int iovcnt) {
    if (fd < 0 || !iov) return -EINVAL;
    if (iovcnt < 0 || iovcnt > UIO_MAXIOV) return -EINVAL;

    struct iovec* kiov = NULL;
    int res = copy_from_user_iovec(iov, iovcnt, &kiov);
    if (res < 0) return res;

    int64_t total = 0;
    for (int i=0; i<iovcnt; i++) {
        int64_t r = sys_read(fd, kiov[i].iov_base, kiov[i].iov_len);
        if (r < 0) {
            kfree(kiov);
            return r;
        }
        total += r;
    }
    kfree(kiov);
    return total;
}

static int64_t sys_fstat(int fd, struct stat* buf) {
    if (!vmm_verify_user_access(buf, sizeof(struct stat), 1)) return -EFAULT;
    task_t* current = task_get_current();
    if (fd < 0 || fd >= MAX_FD) return -EBADF;
    if (!current->fd_table[fd].node) return -EBADF;
    fs_node_t* node = current->fd_table[fd].node;
    memset(buf, 0, sizeof(struct stat));
    buf->st_ino = node->inode; buf->st_size = node->length; buf->st_mode = 0100644;
    if ((node->flags & 0x7) == FS_DIRECTORY) buf->st_mode = 0040755;
    return 0;
}

static int64_t sys_stat(const char* path, struct stat* buf) {
    char* kpath = NULL;
    int res = copy_user_string(path, &kpath, 4096);
    if (res < 0) return res;

    if (!vmm_verify_user_access(buf, sizeof(struct stat), 1)) { kfree(kpath); return -EFAULT; }
    fs_node_t* node = vfs_lookup(fs_root, kpath);
    if (!node) { kfree(kpath); return -ENOENT; }
    memset(buf, 0, sizeof(struct stat));
    buf->st_ino = node->inode; buf->st_size = node->length; buf->st_mode = 0100644;
    if ((node->flags & 0x7) == FS_DIRECTORY) buf->st_mode = 0040755;
    kfree(node);
    kfree(kpath);
    return 0;
}

static int64_t sys_futex(uint32_t *uaddr, int op, uint32_t val, void *timeout, uint32_t *uaddr2, uint32_t val3) {
    (void)uaddr2; (void)val3;
    if (!vmm_verify_user_access(uaddr, 4, 1)) return -EFAULT;

    int cmd = op & FUTEX_CMD_MASK;
    if (cmd == FUTEX_WAIT) {
        if (timeout && !vmm_verify_user_access(timeout, sizeof(struct timespec), 0)) return -EFAULT;
        return futex_wait(uaddr, val, timeout);
    }
    else if (cmd == FUTEX_WAKE) return futex_wake(uaddr, (int)val);
    return -ENOSYS;
}

static int64_t sys_dup2(int oldfd, int newfd) {
    if (oldfd < 0 || oldfd >= MAX_FD || newfd < 0 || newfd >= MAX_FD) return -EBADF;
    task_t* current = task_get_current();
    if (!current->fd_table[oldfd].node) return -EBADF;
    if (oldfd == newfd) return newfd;
    if (current->fd_table[newfd].node) sys_close(newfd);
    current->fd_table[newfd].node = current->fd_table[oldfd].node;
    if (current->fd_table[newfd].node) __atomic_fetch_add(&current->fd_table[newfd].node->ref_count, 1, __ATOMIC_SEQ_CST);
    current->fd_table[newfd].offset = current->fd_table[oldfd].offset;
    current->fd_table[newfd].flags = current->fd_table[oldfd].flags;
    return newfd;
}

static int64_t sys_bind(int fd, const struct sockaddr* addr, int addrlen) {
    if (!vmm_verify_user_access(addr, addrlen, 0)) return -EFAULT;
    if (addrlen < (int)sizeof(struct sockaddr_in)) return -EINVAL;
    task_t* current = task_get_current();
    if (fd < 0 || fd >= MAX_FD) return -EBADF;
    if (!current->fd_table[fd].node) return -EBADF;
    fs_node_t* node = current->fd_table[fd].node;
    if ((node->flags & 0x7) != FS_SOCKET) return -ENOTSOCK;
    struct sockaddr_in* sin = (struct sockaddr_in*)addr;
    if (sin->sin_family != AF_INET) return -EAFNOSUPPORT;
    socket_entry_t* s = get_socket((int)node->inode);
    if (!s) return -EBADF;
    if (s->state != SOCKET_STATE_CLOSED) return -EINVAL;
    s->local_port = ntohs(sin->sin_port);
    return 0;
}

static int64_t sys_listen(int fd, int backlog) {
    (void)backlog;
    task_t* current = task_get_current();
    if (fd < 0 || fd >= MAX_FD) return -EBADF;
    if (!current->fd_table[fd].node) return -EBADF;
    fs_node_t* node = current->fd_table[fd].node;
    if ((node->flags & 0x7) != FS_SOCKET) return -ENOTSOCK;
    socket_entry_t* s = get_socket((int)node->inode);
    if (!s) return -EBADF;
    s->state = SOCKET_STATE_LISTEN;
    return 0;
}

static int64_t sys_accept(int fd, struct sockaddr* addr, int* addrlen) {
    (void)fd;
    if (addr && !vmm_verify_user_access(addr, sizeof(struct sockaddr), 1)) return -EFAULT;
    if (addrlen && !vmm_verify_user_access(addrlen, sizeof(int), 1)) return -EFAULT;
    return -ENOSYS;
}

static int64_t sys_sendto(int fd, const void* buf, size_t len, int flags, const struct sockaddr* dest_addr, int addrlen) {
    if (!vmm_verify_user_access(buf, len, 0)) return -EFAULT;
    if (dest_addr && !vmm_verify_user_access(dest_addr, addrlen, 0)) return -EFAULT;
    task_t* current = task_get_current();
    if (fd < 0 || fd >= MAX_FD) return -EBADF;
    if (!current->fd_table[fd].node) return -EBADF;
    fs_node_t* node = current->fd_table[fd].node;
    if ((node->flags & 0x7) != FS_SOCKET) return -ENOTSOCK;
    int res = net_send((int)node->inode, buf, len, flags);
    if (res < 0) return -EIO;
    return res;
}

static int64_t sys_recvfrom(int fd, void* buf, size_t len, int flags, struct sockaddr* src_addr, int* addrlen) {
    if (!vmm_verify_user_access(buf, len, 1)) return -EFAULT;
    if (src_addr && !vmm_verify_user_access(src_addr, sizeof(struct sockaddr), 1)) return -EFAULT;
    if (addrlen && !vmm_verify_user_access(addrlen, sizeof(int), 1)) return -EFAULT;
    task_t* current = task_get_current();
    if (fd < 0 || fd >= MAX_FD) return -EBADF;
    if (!current->fd_table[fd].node) return -EBADF;
    fs_node_t* node = current->fd_table[fd].node;
    if ((node->flags & 0x7) != FS_SOCKET) return -ENOTSOCK;
    int res = net_recv((int)node->inode, buf, len, flags);
    if (res < 0) return -EIO;
    if (res == 0) return 0;
    return res;
}

struct kernel_sigaction {
    void     (*handler)(int);
    uint64_t  flags;
    void     (*restorer)(void);
    uint64_t  mask;
};

static int64_t sys_rt_sigaction(int sig, const struct kernel_sigaction* act,
                                struct kernel_sigaction* oldact, size_t sigsetsize) {
    (void)sigsetsize;
    if (sig < 1 || sig > 31) return -EINVAL;
    if (sig == SIGKILL || sig == SIGSTOP) return -EINVAL;
    task_t* current = task_get_current();
    if (oldact) {
        if (!vmm_verify_user_access(oldact, sizeof(struct kernel_sigaction), 1)) return -EFAULT;
        oldact->handler = current->signal_handlers[sig];
        oldact->flags = 0;
        oldact->restorer = current->signal_restorers[sig];
        oldact->mask = 0;
    }
    if (act) {
        if (!vmm_verify_user_access(act, sizeof(struct kernel_sigaction), 0)) return -EFAULT;
        current->signal_handlers[sig] = act->handler;
        current->signal_flags[sig] = act->flags;
        /* Store restorer if provided */
        if (act->flags & SA_RESTORER) {
             current->signal_restorers[sig] = act->restorer;
        } else {
             current->signal_restorers[sig] = NULL;
        }
    }
    return 0;
}

static int64_t sys_rt_sigprocmask(int how, const uint64_t* set, uint64_t* oldset, size_t sigsetsize) {
    (void)sigsetsize;
    task_t* current = task_get_current();
    if (oldset) {
        if (!vmm_verify_user_access(oldset, sizeof(uint64_t), 1)) return -EFAULT;
        *oldset = current->signal_mask;
    }
    if (set) {
        if (!vmm_verify_user_access(set, sizeof(uint64_t), 0)) return -EFAULT;
        if (how == 0) current->signal_mask |= (uint32_t)*set;
        else if (how == 1) current->signal_mask &= ~(uint32_t)*set;
        else if (how == 2) current->signal_mask = (uint32_t)*set;
        else return -EINVAL;
        current->signal_mask &= ~((1u << SIGKILL) | (1u << SIGSTOP));
    }
    return 0;
}

/* ========================================================================= */
/* Signal Delivery Logic                                                     */
/* ========================================================================= */

#define SA_SIGINFO 0x00000004

/* Standard siginfo_t layout */
typedef struct {
    int si_signo;
    int si_errno;
    int si_code;
    uint32_t _pad[29];
} siginfo_t;

typedef struct {
    uint64_t uc_flags;
    void*    uc_link;
    uint8_t  uc_stack[24];
    struct syscall_regs uc_mcontext;
    uint32_t uc_sigmask;
} ucontext_t;

/* Signal Frame on User Stack */
struct sigframe {
    void* restorer;
    siginfo_t info;
    ucontext_t uc;
    struct syscall_regs regs;
    uint64_t user_rsp;
    uint32_t sigmask;
};

static void setup_sigcontext(struct syscall_regs* regs, int sig, void* restorer) {
    task_t* current = task_get_current();

    /* Align stack to 16 bytes (System V ABI) */
    /* We subtract frame size and 128 bytes red zone */
    uint64_t sp = current->user_rsp - 128;
    sp = (sp - sizeof(struct sigframe)) & ~0xF;

    /* Verify write access */
    if (!vmm_verify_user_access((void*)sp, sizeof(struct sigframe), 1)) {
        serial_write_string("[SIGNAL] Failed to write stack frame. Terminating.\n");
        task_exit(SIGSEGV);
        return;
    }

    struct sigframe frame;
    memset(&frame, 0, sizeof(frame));
    frame.restorer = restorer;
    frame.regs = *regs;
    frame.user_rsp = current->user_rsp;
    frame.sigmask = current->signal_mask;

    if (current->signal_flags[sig] & SA_SIGINFO) {
        frame.info.si_signo = sig;
        frame.uc.uc_mcontext = *regs;
        frame.uc.uc_sigmask = current->signal_mask;
    }

    /* Copy frame to user stack */
    memcpy((void*)sp, &frame, sizeof(frame));

    /* Update Registers for Handler */
    regs->rdi = sig; /* Arg 1 */
    if (current->signal_flags[sig] & SA_SIGINFO) {
        regs->rsi = sp + offsetof(struct sigframe, info); /* Arg 2 */
        regs->rdx = sp + offsetof(struct sigframe, uc);   /* Arg 3 */
    } else {
        regs->rsi = 0;
        regs->rdx = 0;
    }
    regs->rcx = (uint64_t)current->signal_handlers[sig]; /* RIP (handler) */
    /* Update RSP to point to frame.restorer */
    current->user_rsp = sp;

    /* Update scratch for syscall_entry exit logic */
    cpu_info_t* cpu = get_current_cpu();
    if (cpu) cpu->user_stack_scratch = sp;
}

static void sys_rt_sigreturn(struct syscall_regs* regs) {
    task_t* current = task_get_current();

    uint64_t frame_addr = current->user_rsp;

    /* Verify read access */
    if (!vmm_verify_user_access((void*)frame_addr, sizeof(struct sigframe), 0)) {
        serial_write_string("[SIGNAL] sigreturn fault\n");
        task_exit(SIGSEGV);
        return;
    }

    struct sigframe* frame = (struct sigframe*)frame_addr;

    /* Restore Registers */
    *regs = frame->regs;

    /* Restore Signal Mask */
    current->signal_mask = frame->sigmask;

    /* Restore User RSP */
    current->user_rsp = frame->user_rsp;

    cpu_info_t* cpu = get_current_cpu();
    if (cpu) cpu->user_stack_scratch = current->user_rsp;
}

static void handle_signal(struct syscall_regs* regs) {
    task_t* current = task_get_current();
    if (!current->signal_pending) return;

    /* Find lowest pending signal */
    for (int sig = 1; sig < 32; sig++) {
        if ((current->signal_pending & (1u << sig)) && !(current->signal_mask & (1u << sig))) {
            /* Clear pending */
            current->signal_pending &= ~(1u << sig);

            void (*handler)(int) = current->signal_handlers[sig];

            if (handler == SIG_IGN) {
                continue;
            }
            if (handler == SIG_DFL) {
                serial_write_string("[SIGNAL] Terminating process due to signal\n");
                task_exit(128 + sig);
                return;
            }

            setup_sigcontext(regs, sig, current->signal_restorers[sig]);
            return;
        }
    }
}

static int64_t sys_nanosleep(const struct timespec* req, struct timespec* rem) {
    if (!vmm_verify_user_access(req, sizeof(struct timespec), 0)) return -EFAULT;
    uint64_t ms = (uint64_t)(req->tv_sec * 1000) + (uint64_t)(req->tv_nsec / 1000000);
    if (ms == 0) ms = 1;
    task_sleep(ms);
    if (rem) {
        if (!vmm_verify_user_access(rem, sizeof(struct timespec), 1)) return -EFAULT;
        rem->tv_sec = 0; rem->tv_nsec = 0;
    }
    return 0;
}

static int64_t sys_clock_gettime(int clock_id, struct timespec* tp) {
    (void)clock_id;
    if (!vmm_verify_user_access(tp, sizeof(struct timespec), 1)) return -EFAULT;
    uint32_t uptime = timer_get_uptime_seconds();
    tp->tv_sec = uptime;
    tp->tv_nsec = (timer_get_ticks() % 100) * 10000000;
    return 0;
}

static int64_t sys_getppid(void) { return 1; }
static int64_t sys_gettid(void) { return task_get_current()->id; }
static int64_t sys_set_tid_address(int* tidptr) { (void)tidptr; return task_get_current()->id; }
static int64_t sys_exit_group(int status) { task_exit(status); __builtin_unreachable(); }
static int64_t sys_munmap(void* addr, size_t len) { (void)addr; (void)len; return 0; }
static int64_t sys_mprotect(void* addr, size_t len, int prot) { (void)addr; (void)len; (void)prot; return 0; }
static int64_t sys_madvise(void* addr, size_t len, int advice) { (void)addr; (void)len; (void)advice; return 0; }
static int64_t sys_getuid(void)  { return task_get_current()->uid; }
static int64_t sys_getgid(void)  { return task_get_current()->gid; }
static int64_t sys_geteuid(void) { return task_get_current()->uid; }
static int64_t sys_getegid(void) { return task_get_current()->gid; }
#ifndef F_OK
#define F_OK 0
#define X_OK 1
#define W_OK 2
#define R_OK 4
#endif

static int64_t sys_access(const char* path, int mode) {
    char* kpath = NULL;
    int res = copy_user_string(path, &kpath, 4096);
    if (res < 0) return res;

    fs_node_t* node = vfs_lookup(fs_root, kpath);
    if (!node) { kfree(kpath); return -ENOENT; }

    int allowed = 1;
    if (mode != F_OK) {
        uint32_t req_read = (mode & R_OK) ? 4 : 0;
        uint32_t req_write = (mode & W_OK) ? 2 : 0;
        uint32_t req_exec = (mode & X_OK) ? 1 : 0;
        uint32_t req_mask = req_read | req_write | req_exec;

        uint32_t task_euid = task_get_current()->euid;
        uint32_t task_egid = task_get_current()->egid;

        if (task_euid == 0) {
            /* Root has full access except for execute without any execute bits */
            if (req_exec && !(node->mask & 0111) && !((node->flags & 0x7) == FS_DIRECTORY)) {
                allowed = 0;
            }
        } else {
            uint32_t granted = 0;
            if (task_euid == node->uid) granted = (node->mask >> 6) & 7;
            else if (task_egid == node->gid) granted = (node->mask >> 3) & 7;
            else granted = node->mask & 7;

            if ((granted & req_mask) != req_mask) allowed = 0;
        }
    }

    kfree(node);
    kfree(kpath);
    return allowed ? 0 : -EACCES;
}

static int64_t sys_faccessat(int dirfd, const char* path, int mode, int flags) {
    (void)flags; /* Ignore AT_EACCESS and AT_SYMLINK_NOFOLLOW for now */
    char* kpath = NULL;
    int res = copy_user_string(path, &kpath, 4096);
    if (res < 0) return res;

    task_t* current = task_get_current();
    fs_node_t* base_node;
    if (kpath[0] == '/') {
        base_node = fs_root;
    } else if (dirfd == AT_FDCWD) {
        base_node = current->cwd_node;
    } else {
        if (dirfd < 0 || dirfd >= MAX_FD || !current->fd_table[dirfd].node) {
            kfree(kpath);
            return -EBADF;
        }
        base_node = current->fd_table[dirfd].node;
    }

    fs_node_t* node = vfs_lookup(base_node, kpath);
    if (!node) { kfree(kpath); return -ENOENT; }

    int allowed = 1;
    if (mode != F_OK) {
        uint32_t req_read = (mode & R_OK) ? 4 : 0;
        uint32_t req_write = (mode & W_OK) ? 2 : 0;
        uint32_t req_exec = (mode & X_OK) ? 1 : 0;
        uint32_t req_mask = req_read | req_write | req_exec;

        uint32_t task_euid = current->euid;
        uint32_t task_egid = current->egid;

        if (task_euid == 0) {
            if (req_exec && !(node->mask & 0111) && !((node->flags & 0x7) == FS_DIRECTORY)) {
                allowed = 0;
            }
        } else {
            uint32_t granted = 0;
            if (task_euid == node->uid) granted = (node->mask >> 6) & 7;
            else if (task_egid == node->gid) granted = (node->mask >> 3) & 7;
            else granted = node->mask & 7;

            if ((granted & req_mask) != req_mask) allowed = 0;
        }
    }

    kfree(node);
    kfree(kpath);
    return allowed ? 0 : -EACCES;
}

static int64_t sys_fcntl(int fd, int cmd, int64_t arg) {
    (void)arg;
    task_t* current = task_get_current();
    if (fd < 0 || fd >= MAX_FD) return -EBADF;
    if (cmd == 1) return 0;
    if (cmd == 2) return 0;
    if (cmd == 3) return current->fd_table[fd].flags;
    if (cmd == 4) {
        current->fd_table[fd].flags = (int)arg;
        if (current->fd_table[fd].node) {
            if (arg & O_NONBLOCK)
                current->fd_table[fd].node->flags |= O_NONBLOCK;
            else
                current->fd_table[fd].node->flags &= ~O_NONBLOCK;
        }
        return 0;
    }
    return -EINVAL;
}
static int64_t sys_dup(int oldfd) {
    task_t* current = task_get_current();
    if (oldfd < 0 || oldfd >= MAX_FD) return -EBADF;
    if (!current->fd_table[oldfd].node) return -EBADF;
    for (int i = 0; i < MAX_FD; i++) {
        if (!current->fd_table[i].node) return sys_dup2(oldfd, i);
    }
    return -EMFILE;
}
static int64_t sys_getcwd(char* buf, size_t size) {
    if (!buf || size == 0) return -EINVAL;
    if (!vmm_verify_user_access(buf, size, 1)) return -EFAULT;
    task_t* current = task_get_current();

    char temp_path[256];
    temp_path[0] = '\0';

    fs_node_t* node = current->cwd_node;
    if (!node) {
        /* Fallback */
        if (size < 2) return -ERANGE;
        strlcpy(buf, "/", size);
        return (int64_t)buf;
    }

    /* We need to ascend to root */
    /* Note: Since we don't have a reliable back-link, we'll implement a simple
       VFS traversal upwards. We clone the node so we can safely traverse. */
    fs_node_t* curr_node = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!curr_node) return -ENOMEM;
    memcpy(curr_node, node, sizeof(fs_node_t));

    while (curr_node->inode != fs_root->inode) {
        fs_node_t* parent = vfs_lookup(curr_node, "..");
        if (!parent) {
            /* Error traversing up, break */
            break;
        }

        /* Now find the name of curr_node in parent */
        struct dirent direntry;
        int i = 0;
        int found = 0;
        while (readdir_fs(parent, i, &direntry) != -1) {
            if (direntry.inode == curr_node->inode) {
                found = 1;
                break;
            }
            i++;
        }

        if (found) {
            /* Prepend to temp_path */
            char segment[130];
            strlcpy(segment, "/", sizeof(segment));
            strlcat(segment, direntry.name, sizeof(segment));
            char new_temp[256];
            strlcpy(new_temp, segment, sizeof(new_temp));
            strlcat(new_temp, temp_path, sizeof(new_temp));
            strlcpy(temp_path, new_temp, sizeof(temp_path));
        }

        kfree(curr_node);
        curr_node = parent;
    }

    kfree(curr_node);

    if (temp_path[0] == '\0') {
        strlcpy(temp_path, "/", sizeof(temp_path));
    }

    if (strlen(temp_path) >= size) return -ERANGE;
    strlcpy(buf, temp_path, size);
    return (int64_t)buf;
}

static int64_t sys_chdir(const char* path) {
    char* kpath = NULL;
    int res = copy_user_string(path, &kpath, 4096);
    if (res < 0) return res;

    task_t* current = task_get_current();
    fs_node_t* base_node = fs_root;
    if (kpath[0] != '/') {
        base_node = current->cwd_node;
    }

    fs_node_t* new_cwd = vfs_lookup(base_node, kpath);
    if (!new_cwd) {
        kfree(kpath);
        return -ENOENT;
    }

    if ((new_cwd->flags & 0x7) != FS_DIRECTORY) {
        kfree(new_cwd);
        kfree(kpath);
        return -ENOTDIR;
    }

    if (current->cwd_node) {
        kfree(current->cwd_node);
    }
    current->cwd_node = new_cwd;

    kfree(kpath);
    return 0;
}

static int64_t sys_execve(const char* path, char* const argv[], char* const envp[], struct syscall_regs* regs) {
    return task_exec(path, argv, envp, regs);
}
static int64_t sys_wait4(int pid, int* status, int options, void* rusage) {
    (void)rusage;
    if (status && !vmm_verify_user_access(status, sizeof(int), 1)) return -EFAULT;
    return task_waitpid(pid, status, options);
}


static int64_t sys_sched_yield_wrapper(void) {
    task_yield();
    return 0;
}

static int64_t sys_exit_wrapper(int status) {
    task_exit(status);
    __builtin_unreachable();
}

#define MAX_SYSCALL_NUM 512

typedef int64_t (*syscall_ptr_t)(uint64_t, uint64_t, uint64_t, uint64_t, uint64_t, uint64_t);

static int64_t sys_ni_syscall(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6) {
    (void)a1; (void)a2; (void)a3; (void)a4; (void)a5; (void)a6;
    return -ENOSYS;
}

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-function-type"
#pragma GCC diagnostic ignored "-Woverride-init"

static syscall_ptr_t syscall_table[MAX_SYSCALL_NUM] = {
    [0 ... MAX_SYSCALL_NUM - 1] = sys_ni_syscall,
    [0] = (syscall_ptr_t)sys_read,
    [1] = (syscall_ptr_t)sys_write,
    [2] = (syscall_ptr_t)sys_open,
    [3] = (syscall_ptr_t)sys_close,
    [4] = (syscall_ptr_t)sys_stat,
    [5] = (syscall_ptr_t)sys_fstat,
    [8] = (syscall_ptr_t)sys_lseek,
    [9] = (syscall_ptr_t)sys_mmap,
    [10] = (syscall_ptr_t)sys_mprotect,
    [11] = (syscall_ptr_t)sys_munmap,
    [12] = (syscall_ptr_t)sys_brk,
    [13] = (syscall_ptr_t)sys_rt_sigaction,
    [14] = (syscall_ptr_t)sys_rt_sigprocmask,
    [16] = (syscall_ptr_t)sys_ioctl,
    [19] = (syscall_ptr_t)sys_readv,
    [20] = (syscall_ptr_t)sys_writev,
    [21] = (syscall_ptr_t)sys_access,
    [22] = (syscall_ptr_t)sys_pipe,
    [28] = (syscall_ptr_t)sys_madvise,
    [32] = (syscall_ptr_t)sys_dup,
    [33] = (syscall_ptr_t)sys_dup2,
    [35] = (syscall_ptr_t)sys_nanosleep,
    [39] = (syscall_ptr_t)sys_getpid,
    [41] = (syscall_ptr_t)sys_socket,
    [42] = (syscall_ptr_t)sys_connect,
    [43] = (syscall_ptr_t)sys_accept,
    [44] = (syscall_ptr_t)sys_sendto,
    [45] = (syscall_ptr_t)sys_recvfrom,
    [49] = (syscall_ptr_t)sys_bind,
    [50] = (syscall_ptr_t)sys_listen,
    [61] = (syscall_ptr_t)sys_wait4,
    [62] = (syscall_ptr_t)sys_kill,
    [63] = (syscall_ptr_t)sys_uname,
    [72] = (syscall_ptr_t)sys_fcntl,
    [79] = (syscall_ptr_t)sys_getcwd,
    [80] = (syscall_ptr_t)sys_chdir,
    [83] = (syscall_ptr_t)sys_mkdir,
    [87] = (syscall_ptr_t)sys_unlink,
    [102] = (syscall_ptr_t)sys_getuid,
    [104] = (syscall_ptr_t)sys_getgid,
    [107] = (syscall_ptr_t)sys_geteuid,
    [108] = (syscall_ptr_t)sys_getegid,
    [110] = (syscall_ptr_t)sys_getppid,
    [158] = (syscall_ptr_t)sys_arch_prctl,
    [186] = (syscall_ptr_t)sys_gettid,
    [202] = (syscall_ptr_t)sys_futex,
    [218] = (syscall_ptr_t)sys_set_tid_address,
    [228] = (syscall_ptr_t)sys_clock_gettime,
    [269] = (syscall_ptr_t)sys_faccessat,
    [231] = (syscall_ptr_t)sys_exit_group,
    [24] = (syscall_ptr_t)sys_sched_yield_wrapper,
    [60] = (syscall_ptr_t)sys_exit_wrapper,
};

#pragma GCC diagnostic pop
static void syscall_native_handler(struct syscall_regs* regs) {
    uint64_t ret = (uint64_t)-ENOSYS;
    task_t* current = task_get_current();
    cpu_info_t* cpu = get_current_cpu();
    if (current && cpu) {
        current->user_rsp = cpu->user_stack_scratch;
    }

    if (regs->rax == 0xCAFEBABE) {
        regs->rax = 0;
        return;
    }

    if (regs->rax >= MAX_SYSCALL_NUM) {
        regs->rax = (uint64_t)-ENOSYS;
        return;
    }

    if (regs->rax == SYS_rt_sigreturn) {
        sys_rt_sigreturn(regs);
        return;
    }
    if (regs->rax == SYS_fork || regs->rax == SYS_vfork || regs->rax == SYS_clone) {
        regs->rax = (uint64_t)task_fork((void*)regs);
        return;
    }
    if (regs->rax == SYS_execve) {
        regs->rax = (uint64_t)sys_execve((const char*)regs->rdi, (char* const*)regs->rsi, (char* const*)regs->rdx, regs);
        return;
    }

    syscall_ptr_t handler = syscall_table[regs->rax];
    if (handler) {
        ret = handler(regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8, regs->r9);
    }

    regs->rax = ret;
    handle_signal(regs);
}

static void syscall_linux_handler(struct syscall_regs* regs) {
    syscall_native_handler(regs);
}

void syscall_handler(struct syscall_regs* regs) {
    task_t* current = task_get_current();
    if (current && current->os_abi == ELFOSABI_LINUX) {
        syscall_linux_handler(regs);
    } else {
        syscall_native_handler(regs);
    }
}
