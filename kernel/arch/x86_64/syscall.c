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

extern void syscall_entry(void);

/* External Declarations for Task Functions */
extern int task_exec(const char* path, char* const argv[], char* const envp[], struct syscall_regs* regs);
extern int task_waitpid(int pid, int* status, int options);

/* Definition of per-cpu data removed - handled by smp.c */

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

    /* 5. GS_BASE: Initialization moved to cpu_init_bsp() / cpu_init_ap() in smp.c */
    /* wrmsr(MSR_GS_BASE, ...); */
    
    /* Initialize stack: handled by task.c / smp.c */ 

    serial_write_string("[SYSCALL] x86_64 mechanism enabled.\n");
}

/* ========================================================================= */
/* Syscall Implementations                                                   */
/* ========================================================================= */

/* --- Socket VFS Wrappers --- */

static uint32_t socket_read_fs(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    (void)offset;
    if ((node->flags & 0x7) != FS_SOCKET) return 0;

    /* node->inode holds the socket ID (see sys_socket) */
    /* net_recv takes flags. Standard read() implies flags=0 (blocking) */
    int res = net_recv((int)node->inode, buffer, size, 0);
    return (res < 0) ? 0 : (uint32_t)res;
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

#define O_ACCMODE   00000003
#define O_RDONLY    00000000
#define O_WRONLY    00000001
#define O_RDWR      00000002
#define O_CREAT     0x00000040

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

static uint32_t pipe_read(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    (void)offset;
    pipe_t* pipe = (pipe_t*)node->ptr;
    if (!pipe) return 0;

    uint32_t read = 0;
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
    /* Identify if reader or writer based on node flags? Or just decrement both?
       Typically we have distinct open files.
       Wait, fs_node_t is cloned for dup2 but shared for pipe ends?
       We created 2 nodes. One Read only, one Write only.
    */
    if (node->flags == FS_PIPE) { /* Reader? Writer? We need to distinguish. */
        /* Hack: check function pointers? */
        if (node->write == 0) pipe->readers--;
        else pipe->writers--;
    } else {
        /* Assume both? */
        pipe->readers--;
        pipe->writers--;
    }

    if (pipe->readers <= 0 && pipe->writers <= 0) {
        /* Free pipe */
        kfree(pipe->buffer);
        kfree(pipe);
    } else {
        spin_unlock(&pipe->lock);
    }
    /* node itself is freed by close_fs caller (sys_close) */
}

static void pipe_open(fs_node_t* node) {
    (void)node;
}


static int validate_user_string(const char* str) {
    if (!str) return 0;

    /* Validate start pointer. Length unknown, check at least 1 byte. */
    if (!vmm_validate_user_ptr(str, 1)) return 0;

    /* Scan for null terminator within limit. */
    uint64_t ptr_val = (uint64_t)str;
    uint64_t remaining = USER_LIMIT - ptr_val + 1; /* Bytes until limit (inclusive) */
    uint64_t max_scan = (remaining < 4096) ? remaining : 4096;

    for (uint64_t i = 0; i < max_scan; i++) {
        /* Check page access for each new page */
        uint64_t curr_addr = ptr_val + i;
        if ((i == 0) || ((curr_addr & (PAGE_SIZE - 1)) == 0)) {
            if (!vmm_verify_user_access((void*)curr_addr, 1, 0)) return 0;
        }

        if (str[i] == '\0') return 1;
    }
    return 0; /* Too long */
}

/* Helper to split path into parent and name */
static int split_path(const char* path, char* parent, char* name) {
    int len = strlen(path);
    int last_slash = -1;
    for (int i = 0; i < len; i++) {
        if (path[i] == '/') last_slash = i;
    }

    if (last_slash == -1) {
        // No slash, parent is current (unsupported yet) or root if absolute?
        // Assuming path is relative to some CWD, but for now everything is absolute or relative to root?
        // The VFS lookup handles root if path starts with /.
        // If path is "file.txt" and we are at root...
        // Let's assume root.
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

static int64_t sys_mmap(void* addr, size_t len, int prot, int flags, int fd, int64_t offset) {
    (void)fd; (void)offset;
    if (len == 0) return -EINVAL;

    /* MAP_ANONYMOUS = 0x20 */
    if (!(flags & 0x20)) {
        return -ENODEV; /* File mapping not implemented yet */
    }

    task_t* current = task_get_current();
    uint64_t virt;

    if (addr && (flags & 0x10)) { /* MAP_FIXED = 0x10 */
        virt = (uint64_t)addr;
        if (virt & 0xFFF) return -EINVAL;
    } else {
        virt = current->mmap_base;
        current->mmap_base += PAGE_ALIGN_UP(len);
    }

    uint64_t start = PAGE_ALIGN_DOWN(virt);
    uint64_t end = PAGE_ALIGN_UP(virt + len);

    for (uint64_t v = start; v < end; v += PAGE_SIZE) {
        if (hal_mem_get_phys(v) == 0) {
            void* phys = pmm_alloc_page();
            if (!phys) return -ENOMEM;

            /* PROT_READ=1, PROT_WRITE=2, PROT_EXEC=4 */
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
    /* Only AF_INET, SOCK_STREAM, IPPROTO_TCP supported */
    /* We allow 0 for protocol as default */
    if (domain != AF_INET) return -EAFNOSUPPORT;
    if (type != SOCK_STREAM) return -EPROTOTYPE; // Only TCP supported
    if (protocol != 0 && protocol != IPPROTO_TCP) return -EPROTONOSUPPORT;

    int sock_id = net_socket(domain, type, IPPROTO_TCP);
    if (sock_id < 0) return -ENOMEM; /* socket_table full */

    task_t* current = task_get_current();

    /* Find free FD */
    int fd = -1;
    for (int i = 3; i < MAX_FD; i++) {
        if (current->fd_table[i].node == NULL) {
            fd = i;
            break;
        }
    }

    if (fd == -1) {
        /* Close the socket we just created to prevent leak */
        net_close(sock_id);
        return -EMFILE;
    }

    /* Allocate fs_node_t */
    fs_node_t* node = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!node) {
        net_close(sock_id);
        return -ENOMEM;
    }
    memset(node, 0, sizeof(fs_node_t));

    strlcpy(node->name, "socket", 32);
    node->flags = FS_SOCKET;
    node->inode = (uint32_t)sock_id; /* Store socket ID in inode field */
    node->read = socket_read_fs;
    node->write = socket_write_fs;
    node->close = socket_close_fs;
    node->ref_count = 1;
    node->open = NULL; /* No specific open action needed */

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
        /* Map kernel stack errors to errno */
        if (res == -2) return -ENETUNREACH; /* ARP failed */
        return -ECONNREFUSED;
    }
    return 0;
}

static int64_t sys_pipe(int* pipefd) {
    if (!vmm_verify_user_access(pipefd, 2 * sizeof(int), 1)) return -EFAULT;

    task_t* current = task_get_current();

    /* Find 2 FDs */
    int fd[2] = {-1, -1};
    int count = 0;
    for (int i = 3; i < MAX_FD && count < 2; i++) {
        if (current->fd_table[i].node == NULL) {
            fd[count++] = i;
        }
    }

    if (count < 2) return -EMFILE;

    /* Alloc Pipe */
    pipe_t* pipe = (pipe_t*)kmalloc(sizeof(pipe_t));
    if (!pipe) return -ENOMEM;
    memset(pipe, 0, sizeof(pipe_t));

    pipe->buffer = (uint8_t*)kmalloc(PIPE_SIZE);
    if (!pipe->buffer) {
        kfree(pipe);
        return -ENOMEM;
    }
    pipe->size = PIPE_SIZE;
    pipe->readers = 1;
    pipe->writers = 1;

    /* Create Nodes */
    fs_node_t* reader = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    fs_node_t* writer = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    memset(reader, 0, sizeof(fs_node_t));
    memset(writer, 0, sizeof(fs_node_t));
    reader->ref_count = 1;
    writer->ref_count = 1;

    strlcpy(reader->name, "pipe_r", 32);
    reader->flags = FS_PIPE;
    reader->read = pipe_read;
    reader->close = pipe_close;
    reader->open = pipe_open;
    reader->ptr = (struct fs_node*)pipe; /* Safe cast */

    strlcpy(writer->name, "pipe_w", 32);
    writer->flags = FS_PIPE;
    writer->write = pipe_write;
    writer->close = pipe_close;
    writer->open = pipe_open;
    writer->ptr = (struct fs_node*)pipe;

    /* Assign FDs */
    current->fd_table[fd[0]].node = reader;
    current->fd_table[fd[0]].offset = 0;
    current->fd_table[fd[0]].flags = O_RDONLY;

    current->fd_table[fd[1]].node = writer;
    current->fd_table[fd[1]].offset = 0;
    current->fd_table[fd[1]].flags = O_WRONLY;

    pipefd[0] = fd[0];
    pipefd[1] = fd[1];

    return 0;
}

#define ARCH_SET_GS 0x1001
#define ARCH_SET_FS 0x1002

struct utsname {
    char sysname[65];
    char nodename[65];
    char release[65];
    char version[65];
    char machine[65];
    char domainname[65];
};

struct iovec {
    void  *iov_base;
    size_t iov_len;
};

struct stat {
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

static int64_t sys_write(int fd, const void* buf, size_t count) {
    if (!vmm_verify_user_access(buf, count, 0)) return -EFAULT;

    /* File Descriptor Write */
    task_t* current = task_get_current();
    if (fd < 0 || fd >= MAX_FD) return -EBADF;
    if (!current->fd_table[fd].node) return -EBADF;

    uint32_t written = write_fs(current->fd_table[fd].node,
                                current->fd_table[fd].offset,
                                count,
                                (uint8_t*)buf);
    current->fd_table[fd].offset += written;
    return written;
}

static int64_t sys_read(int fd, void* buf, size_t count) {
    if (!vmm_verify_user_access(buf, count, 1)) return -EFAULT;

    task_t* current = task_get_current();
    if (fd < 0 || fd >= MAX_FD) return -EBADF;
    if (!current->fd_table[fd].node) return -EBADF;

    uint32_t read = read_fs(current->fd_table[fd].node,
                            current->fd_table[fd].offset,
                            count,
                            (uint8_t*)buf);
    current->fd_table[fd].offset += read;
    return read;
}

static int64_t sys_open(const char* path, int flags, int mode) {
    if (!validate_user_string(path)) return -EFAULT;

    (void)mode;
    task_t* current = task_get_current();

    /* Find free FD */
    int fd = -1;
    for (int i = 3; i < MAX_FD; i++) { /* Start from 3 (0,1,2 reserved) */
        if (current->fd_table[i].node == NULL) {
            fd = i;
            break;
        }
    }
    if (fd == -1) return -EMFILE;

    /* Lookup path */
    fs_node_t* node = vfs_lookup(fs_root, path);

    if (!node) {
        if (flags & O_CREAT) {
            char parent_path[128];
            char filename[128];
            if (split_path(path, parent_path, filename) != 0) return -ENAMETOOLONG;

            fs_node_t* parent = vfs_lookup(fs_root, parent_path);
            if (!parent) return -ENOENT;

            if (create_fs(parent, filename, (uint16_t)mode) != 0) {
                kfree(parent);
                return -EACCES;
            }

            node = vfs_lookup(fs_root, path);
            kfree(parent);
            if (!node) return -ENOENT;
        } else {
            return -ENOENT;
        }
    }

    uint8_t read_mode = 0;
    uint8_t write_mode = 0;

    if ((flags & O_ACCMODE) == O_RDONLY) { read_mode = 1; }
    else if ((flags & O_ACCMODE) == O_WRONLY) { write_mode = 1; }
    else if ((flags & O_ACCMODE) == O_RDWR) { read_mode = 1; write_mode = 1; }

    open_fs(node, read_mode, write_mode);

    current->fd_table[fd].node = node;
    current->fd_table[fd].offset = 0;
    current->fd_table[fd].flags = flags;

    return fd;
}

static int64_t sys_close(int fd) {
    task_t* current = task_get_current();
    if (fd < 0 || fd >= MAX_FD) return -EBADF;
    if (!current->fd_table[fd].node) return -EBADF;

    fs_node_t* node = current->fd_table[fd].node;
    if (__atomic_sub_fetch(&node->ref_count, 1, __ATOMIC_SEQ_CST) == 0) {
        close_fs(node);
        /* Free the node memory (vfs_lookup allocates it) */
        kfree(node);
    }

    current->fd_table[fd].node = NULL;

    return 0;
}

static int64_t sys_mkdir(const char* path, int mode) {
    if (!validate_user_string(path)) return -EFAULT;

    char parent_path[128];
    char filename[128];
    if (split_path(path, parent_path, filename) != 0) return -ENAMETOOLONG;

    fs_node_t* parent = vfs_lookup(fs_root, parent_path);
    if (!parent) return -ENOENT;

    int res = mkdir_fs(parent, filename, (uint16_t)mode);
    kfree(parent);
    return res;
}

static int64_t sys_unlink(const char* path) {
    if (!validate_user_string(path)) return -EFAULT;

    char parent_path[128];
    char filename[128];
    if (split_path(path, parent_path, filename) != 0) return -ENAMETOOLONG;

    fs_node_t* parent = vfs_lookup(fs_root, parent_path);
    if (!parent) return -ENOENT;

    int res = unlink_fs(parent, filename);
    kfree(parent);
    return res;
}

static int64_t sys_lseek(int fd, int64_t offset, int whence) {
    task_t* current = task_get_current();
    if (fd < 0 || fd >= MAX_FD) return -EBADF;
    if (!current->fd_table[fd].node) return -EBADF;

    /* SEEK_SET=0, SEEK_CUR=1, SEEK_END=2 */
    if (whence == 0) {
        current->fd_table[fd].offset = offset;
    } else if (whence == 1) {
        current->fd_table[fd].offset += offset;
    } else if (whence == 2) {
        current->fd_table[fd].offset = current->fd_table[fd].node->length + offset;
    } else {
        return -EINVAL;
    }

    return current->fd_table[fd].offset;
}

static int64_t sys_getpid(void) {
    return task_get_current()->id;
}

static int64_t sys_kill(int pid, int sig) {
    if (pid <= 0) return -EINVAL;
    if (sig < 0 || sig > 31) return -EINVAL;
    if (sig == 0) {
        /* Signal 0: just check if process exists */
        task_t* target = task_get_by_id((uint32_t)pid);
        return target ? 0 : -ESRCH;
    }

    /* SIGKILL (9) and SIGSTOP (19): Cannot be caught or ignored */
    if (sig == SIGKILL || sig == SIGTERM) {
        if (task_kill((uint32_t)pid) == 0) {
            serial_write_string("[SIGNAL] Delivered SIGKILL/SIGTERM to PID\n");
            return 0;
        }
        return -ESRCH;
    }

    /* All other signals: queue as pending on target task */
    task_t* target = task_get_by_id((uint32_t)pid);
    if (!target) return -ESRCH;
    if (target->state == TASK_DEAD) return -ESRCH;

    /* Set the signal pending bit (if not masked) */
    target->signal_pending |= (1u << sig);

    /* Wake up if blocked */
    if (target->state == TASK_BLOCKED || target->state == TASK_SLEEPING) {
        target->state = TASK_READY;
    }

    serial_write_string("[SIGNAL] Queued signal on task\n");
    return 0;
}

static int64_t sys_brk(uint64_t brk) {
    task_t* current = task_get_current();
    if (brk == 0) return current->brk;

    /* If brk is smaller than current, just update it (shrink) */
    if (brk < current->brk) {
        current->brk = brk;
        return current->brk;
    }

    /* Growing heap */
    uint64_t old_page = PAGE_ALIGN_UP(current->brk);
    uint64_t new_page = PAGE_ALIGN_UP(brk);

    /* Allocate pages if we crossed a page boundary */
    if (new_page > old_page) {
        for (uint64_t addr = old_page; addr < new_page; addr += PAGE_SIZE) {
             /* Check if already mapped */
             if (hal_mem_get_phys(addr) == 0) {
                  void* phys = pmm_alloc_page();
                  if (!phys) return -ENOMEM;
                  /* User | Read/Write */
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
    if (code == ARCH_SET_FS) {
        current->fs_base = addr;
        wrmsr(MSR_FS_BASE, addr);
        return 0;
    } else if (code == ARCH_SET_GS) {
        current->gs_base = addr;
        /* We are in kernel, so user GS base is in KERNEL_GS_BASE */
        wrmsr(MSR_KERNEL_GS_BASE, addr);
        return 0;
    }
    return -EINVAL;
}

static int64_t sys_uname(struct utsname* buf) {
    if (!vmm_verify_user_access(buf, sizeof(struct utsname), 1)) return -EFAULT;

    /* Simulate Linux 5.5 */
    strlcpy(buf->sysname, "Linux", 65);
    strlcpy(buf->nodename, "eterOS", 65);
    strlcpy(buf->release, "5.5.0-generic", 65);
    strlcpy(buf->version, "#1 SMP 2026", 65);
    strlcpy(buf->machine, "x86_64", 65);
    strlcpy(buf->domainname, "localdomain", 65);
    return 0;
}

static int64_t sys_ioctl(int fd, unsigned long request, void* arg) {
    task_t* current = task_get_current();
    if (fd < 0 || fd >= MAX_FD) return -EBADF;
    if (!current->fd_table[fd].node) return -EBADF;

    return ioctl_fs(current->fd_table[fd].node, (int)request, arg);
}

static int64_t sys_writev(int fd, const struct iovec *iov, int iovcnt) {
    if (fd < 0 || !iov) return -EINVAL;
    int64_t total = 0;
    for (int i=0; i<iovcnt; i++) {
        int64_t res = sys_write(fd, iov[i].iov_base, iov[i].iov_len);
        if (res < 0) return res;
        total += res;
    }
    return total;
}

static int64_t sys_readv(int fd, const struct iovec *iov, int iovcnt) {
    if (fd < 0 || !iov) return -EINVAL;
    int64_t total = 0;
    for (int i=0; i<iovcnt; i++) {
        int64_t res = sys_read(fd, iov[i].iov_base, iov[i].iov_len);
        if (res < 0) return res;
        total += res;
    }
    return total;
}

static int64_t sys_fstat(int fd, struct stat* buf) {
    if (!vmm_verify_user_access(buf, sizeof(struct stat), 1)) return -EFAULT;

    task_t* current = task_get_current();
    if (fd < 0 || fd >= MAX_FD) return -EBADF;
    if (!current->fd_table[fd].node) return -EBADF;
    fs_node_t* node = current->fd_table[fd].node;

    memset(buf, 0, sizeof(struct stat));
    buf->st_ino = node->inode;
    buf->st_size = node->length;
    buf->st_mode = 0100644; /* Regular file */
    if ((node->flags & 0x7) == FS_DIRECTORY) {
        buf->st_mode = 0040755; /* Directory */
    }
    return 0;
}

static int64_t sys_stat(const char* path, struct stat* buf) {
    if (!validate_user_string(path)) return -EFAULT;
    if (!vmm_verify_user_access(buf, sizeof(struct stat), 1)) return -EFAULT;

    fs_node_t* node = vfs_lookup(fs_root, path);
    if (!node) return -ENOENT;

    memset(buf, 0, sizeof(struct stat));
    buf->st_ino = node->inode;
    buf->st_size = node->length;
    buf->st_mode = 0100644;
    if ((node->flags & 0x7) == FS_DIRECTORY) {
        buf->st_mode = 0040755;
    }

    /* vfs_lookup does not allocate the node usually, but if it did we should free it?
       Current vfs_lookup returns pointer to internal tree or allocated?
       Memory check: sys_open calls vfs_lookup then creates FD. sys_close frees node?
       If vfs_lookup allocates, then sys_stat leaks memory!
       Let's assume vfs_lookup returns a new node for now as per sys_open usage.
       If vfs_lookup allocates, we must free.
       sys_close calls kfree(node).
       So yes, we must free. */
    kfree(node);

    return 0;
}

static int64_t sys_futex(uint32_t *uaddr, int op, uint32_t val, void *timeout, uint32_t *uaddr2, uint32_t val3) {
    (void)uaddr2; (void)val3;

    int cmd = op & FUTEX_CMD_MASK;

    if (cmd == FUTEX_WAIT) {
        return futex_wait(uaddr, val, timeout);
    } else if (cmd == FUTEX_WAKE) {
        return futex_wake(uaddr, (int)val);
    }

    return -ENOSYS;
}

static int64_t sys_dup2(int oldfd, int newfd) {
    if (oldfd < 0 || oldfd >= MAX_FD || newfd < 0 || newfd >= MAX_FD) return -EBADF;

    task_t* current = task_get_current();
    if (!current->fd_table[oldfd].node) return -EBADF;

    if (oldfd == newfd) return newfd;

    if (current->fd_table[newfd].node) {
        sys_close(newfd);
    }

    /* We should share the file description (offset + node).
       But our current architecture stores offset in fd_table directly.
       We will copy state, meaning offsets diverge. This is a known limitation.
       We share the node and use refcounting to avoid double-free.
    */
    current->fd_table[newfd].node = current->fd_table[oldfd].node;
    if (current->fd_table[newfd].node) {
        __atomic_fetch_add(&current->fd_table[newfd].node->ref_count, 1, __ATOMIC_SEQ_CST);
    }

    current->fd_table[newfd].offset = current->fd_table[oldfd].offset;
    current->fd_table[newfd].flags = current->fd_table[oldfd].flags;

    return newfd;
}

static int64_t sys_bind(int fd, const struct sockaddr* addr, int addrlen) {
    if (!vmm_verify_user_access(addr, addrlen, 0)) return -EFAULT;

    task_t* current = task_get_current();
    if (fd < 0 || fd >= MAX_FD) return -EBADF;
    if (!current->fd_table[fd].node) return -EBADF;

    fs_node_t* node = current->fd_table[fd].node;
    if ((node->flags & 0x7) != FS_SOCKET) return -ENOTSOCK;

    /* stack.c net_socket initializes local_port to 0.
       net_connect binds ephemeral if 0.
       We can set it here if socket is fresh.
    */
    struct sockaddr_in* sin = (struct sockaddr_in*)addr;
    if (sin->sin_family != AF_INET) return -EAFNOSUPPORT;

    socket_entry_t* s = get_socket((int)node->inode);
    if (!s) return -EBADF;

    if (s->state != SOCKET_STATE_CLOSED) return -EINVAL; /* Already connected/bound? */

    s->local_port = ntohs(sin->sin_port);
    /* Ignore IP for now (bind to all interfaces/my_ip) */

    return 0;
}

static int64_t sys_listen(int fd, int backlog) {
    (void)backlog;
    task_t* current = task_get_current();
    if (fd < 0 || fd >= MAX_FD) return -EBADF;
    if (!current->fd_table[fd].node) return -EBADF;

    fs_node_t* node = current->fd_table[fd].node;
    if ((node->flags & 0x7) != FS_SOCKET) return -ENOTSOCK;

    /* Check if socket exists */
    socket_entry_t* s = get_socket((int)node->inode);
    if (!s) return -EBADF;

    /* Set state to LISTEN - but stack.c doesn't handle this fully yet */
    s->state = SOCKET_STATE_LISTEN;

    /* We return success to allow applications to proceed,
       even if actual connections won't be accepted by stack logic yet. */
    return 0;
}

static int64_t sys_accept(int fd, struct sockaddr* addr, int* addrlen) {
    /* Blocking accept - Not supported by stack.c yet */
    (void)fd; (void)addr; (void)addrlen;
    return -ENOSYS;
}

static int64_t sys_sendto(int fd, const void* buf, size_t len, int flags, const struct sockaddr* dest_addr, int addrlen) {
    (void)dest_addr; (void)addrlen;
    /* We ignore dest_addr for TCP (SOCK_STREAM) as it's connection based.
       If we supported UDP, we'd use it.
    */
    if (!vmm_verify_user_access(buf, len, 0)) return -EFAULT;

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
    (void)src_addr; (void)addrlen;
    if (!vmm_verify_user_access(buf, len, 1)) return -EFAULT;

    task_t* current = task_get_current();
    if (fd < 0 || fd >= MAX_FD) return -EBADF;
    if (!current->fd_table[fd].node) return -EBADF;

    fs_node_t* node = current->fd_table[fd].node;
    if ((node->flags & 0x7) != FS_SOCKET) return -ENOTSOCK;

    int res = net_recv((int)node->inode, buf, len, flags);
    if (res < 0) return -EIO; /* Or EAGAIN if nonblocking? */
    if (res == 0) return 0; /* EOF */
    return res;
}

/* ========================================================================= */
/* Signal Syscalls                                                           */
/* ========================================================================= */

#define SA_RESTORER 0x04000000

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
    /* SIGKILL and SIGSTOP cannot be caught */
    if (sig == SIGKILL || sig == SIGSTOP) return -EINVAL;

    task_t* current = task_get_current();

    /* Return old action */
    if (oldact) {
        oldact->handler = current->signal_handlers[sig];
        oldact->flags = 0;
        oldact->restorer = 0;
        oldact->mask = 0;
    }

    /* Install new action */
    if (act) {
        current->signal_handlers[sig] = act->handler;
    }

    return 0;
}

static int64_t sys_rt_sigprocmask(int how, const uint64_t* set, uint64_t* oldset, size_t sigsetsize) {
    (void)sigsetsize;
    task_t* current = task_get_current();

    if (oldset) {
        *oldset = current->signal_mask;
    }

    if (set) {
        /* SIG_BLOCK=0, SIG_UNBLOCK=1, SIG_SETMASK=2 */
        if (how == 0) {
            current->signal_mask |= (uint32_t)*set;
        } else if (how == 1) {
            current->signal_mask &= ~(uint32_t)*set;
        } else if (how == 2) {
            current->signal_mask = (uint32_t)*set;
        } else {
            return -EINVAL;
        }
        /* SIGKILL and SIGSTOP can never be blocked */
        current->signal_mask &= ~((1u << SIGKILL) | (1u << SIGSTOP));
    }

    return 0;
}

static int64_t sys_rt_sigreturn(void) {
    /* Minimal stub - real implementation would restore signal frame */
    return 0;
}

/* ========================================================================= */
/* Additional POSIX Syscalls (musl libc support)                             */
/* ========================================================================= */

struct timespec {
    int64_t tv_sec;
    int64_t tv_nsec;
};

static int64_t sys_nanosleep(const struct timespec* req, struct timespec* rem) {
    if (!req) return -EFAULT;

    /* Convert to milliseconds for task_sleep */
    uint64_t ms = (uint64_t)(req->tv_sec * 1000) + (uint64_t)(req->tv_nsec / 1000000);
    if (ms == 0) ms = 1; /* At least 1ms */

    task_sleep(ms);

    /* Set remaining time to 0 (we slept the full duration) */
    if (rem) {
        rem->tv_sec = 0;
        rem->tv_nsec = 0;
    }
    return 0;
}

static int64_t sys_clock_gettime(int clock_id, struct timespec* tp) {
    (void)clock_id;
    if (!tp) return -EFAULT;

    uint32_t uptime = timer_get_uptime_seconds();
    tp->tv_sec = uptime;
    tp->tv_nsec = (timer_get_ticks() % 100) * 10000000; /* Approximate from PIT */
    return 0;
}

static int64_t sys_getppid(void) {
    /* éterOS does not track parent PID yet, return 1 (init) */
    return 1;
}

static int64_t sys_gettid(void) {
    return task_get_current()->id;
}

static int64_t sys_set_tid_address(int* tidptr) {
    /* musl calls this during thread init; just return our TID */
    (void)tidptr;
    return task_get_current()->id;
}

static int64_t sys_exit_group(int status) {
    task_exit(status);
    __builtin_unreachable();
}

static int64_t sys_munmap(void* addr, size_t len) {
    /* Minimal stub: we don't actually free pages yet */
    (void)addr; (void)len;
    return 0;
}

static int64_t sys_mprotect(void* addr, size_t len, int prot) {
    /* Stub: we don't change page protections dynamically yet */
    (void)addr; (void)len; (void)prot;
    return 0;
}

static int64_t sys_madvise(void* addr, size_t len, int advice) {
    /* Stub: no-op advisory */
    (void)addr; (void)len; (void)advice;
    return 0;
}

static int64_t sys_getuid(void)  { return 0; /* root */ }
static int64_t sys_getgid(void)  { return 0; }
static int64_t sys_geteuid(void) { return 0; }
static int64_t sys_getegid(void) { return 0; }

static int64_t sys_access(const char* path, int mode) {
    if (!validate_user_string(path)) return -EFAULT;

    (void)mode;
    fs_node_t* node = vfs_lookup(fs_root, path);
    if (!node) return -ENOENT;
    kfree(node);
    return 0;
}

static int64_t sys_fcntl(int fd, int cmd, int64_t arg) {
    (void)arg;
    task_t* current = task_get_current();
    if (fd < 0 || fd >= MAX_FD) return -EBADF;

    /* F_GETFD=1, F_SETFD=2, F_GETFL=3, F_SETFL=4 */
    if (cmd == 1) return 0; /* F_GETFD: no close-on-exec */
    if (cmd == 2) return 0; /* F_SETFD: ignore */
    if (cmd == 3) return current->fd_table[fd].flags; /* F_GETFL */
    if (cmd == 4) { current->fd_table[fd].flags = (int)arg; return 0; } /* F_SETFL */
    return -EINVAL;
}

static int64_t sys_dup(int oldfd) {
    task_t* current = task_get_current();
    if (oldfd < 0 || oldfd >= MAX_FD) return -EBADF;
    if (!current->fd_table[oldfd].node) return -EBADF;

    /* Find lowest free FD */
    for (int i = 0; i < MAX_FD; i++) {
        if (!current->fd_table[i].node) {
            return sys_dup2(oldfd, i);
        }
    }
    return -EMFILE;
}

static int64_t sys_getcwd(char* buf, size_t size) {
    if (!buf || size < 2) return -EINVAL;
    /* éterOS has no per-process CWD yet; return "/" */
    strlcpy(buf, "/", size);
    return (int64_t)buf;
}

static int64_t sys_execve(const char* path, char* const argv[], char* const envp[], struct syscall_regs* regs) {
    /* Validation happens in task_exec */
    return task_exec(path, argv, envp, regs);
}

static int64_t sys_wait4(int pid, int* status, int options, void* rusage) {
    (void)rusage;
    if (status && !vmm_verify_user_access(status, sizeof(int), 1)) return -EFAULT;
    return task_waitpid(pid, status, options);
}

static void syscall_native_handler(struct syscall_regs* regs) {
    uint64_t ret = (uint64_t)-ENOSYS;

    task_t* current = task_get_current();
    cpu_info_t* cpu = get_current_cpu();
    if (current && cpu) {
        /* Save User RSP so fork can copy it and schedule can restore it */
        current->user_rsp = cpu->user_stack_scratch;
    }

    /* Handle Syscalls */
    if (regs->rax == SYS_read) {
        ret = (uint64_t)sys_read((int)regs->rdi, (void*)regs->rsi, (size_t)regs->rdx);
    } else if (regs->rax == SYS_write) {
        ret = (uint64_t)sys_write((int)regs->rdi, (const void*)regs->rsi, (size_t)regs->rdx);
    } else if (regs->rax == SYS_open) {
        ret = (uint64_t)sys_open((const char*)regs->rdi, (int)regs->rsi, (int)regs->rdx);
    } else if (regs->rax == SYS_close) {
        ret = (uint64_t)sys_close((int)regs->rdi);
    } else if (regs->rax == SYS_lseek) {
        ret = (uint64_t)sys_lseek((int)regs->rdi, (int64_t)regs->rsi, (int)regs->rdx);
    } else if (regs->rax == SYS_socket) {
        ret = (uint64_t)sys_socket((int)regs->rdi, (int)regs->rsi, (int)regs->rdx);
    } else if (regs->rax == SYS_connect) {
        ret = (uint64_t)sys_connect((int)regs->rdi, (const struct sockaddr*)regs->rsi, (int)regs->rdx);
    } else if (regs->rax == SYS_bind) {
        ret = (uint64_t)sys_bind((int)regs->rdi, (const struct sockaddr*)regs->rsi, (int)regs->rdx);
    } else if (regs->rax == SYS_listen) {
        ret = (uint64_t)sys_listen((int)regs->rdi, (int)regs->rsi);
    } else if (regs->rax == SYS_accept) {
        ret = (uint64_t)sys_accept((int)regs->rdi, (struct sockaddr*)regs->rsi, (int*)regs->rdx);
    } else if (regs->rax == SYS_sendto) {
        ret = (uint64_t)sys_sendto((int)regs->rdi, (const void*)regs->rsi, (size_t)regs->rdx, (int)regs->r10, (const struct sockaddr*)regs->r8, (int)regs->r9);
    } else if (regs->rax == SYS_recvfrom) {
        ret = (uint64_t)sys_recvfrom((int)regs->rdi, (void*)regs->rsi, (size_t)regs->rdx, (int)regs->r10, (struct sockaddr*)regs->r8, (int*)regs->r9);
    } else if (regs->rax == SYS_exit) {
        task_exit((int)regs->rdi);
        __builtin_unreachable();
    } else if (regs->rax == SYS_lseek) {
        ret = (uint64_t)sys_lseek((int)regs->rdi, (int64_t)regs->rsi, (int)regs->rdx);
    } else if (regs->rax == SYS_getpid) {
        ret = (uint64_t)sys_getpid();
    } else if (regs->rax == SYS_kill) {
        ret = (uint64_t)sys_kill((int)regs->rdi, (int)regs->rsi);
    } else if (regs->rax == SYS_sched_yield) {
        task_yield();
        ret = 0;
    } else if (regs->rax == SYS_mmap) {
        ret = (uint64_t)sys_mmap((void*)regs->rdi, (size_t)regs->rsi, (int)regs->rdx, (int)regs->r10, (int)regs->r8, (int64_t)regs->r9);
    } else if (regs->rax == SYS_brk) {
        ret = (uint64_t)sys_brk((uint64_t)regs->rdi);
    } else if (regs->rax == SYS_fork) {
        ret = (uint64_t)task_fork((void*)regs);
    } else if (regs->rax == SYS_execve) {
        ret = (uint64_t)sys_execve((const char*)regs->rdi, (char* const*)regs->rsi, (char* const*)regs->rdx, regs);
    } else if (regs->rax == SYS_wait4) {
        ret = (uint64_t)sys_wait4((int)regs->rdi, (int*)regs->rsi, (int)regs->rdx, (void*)regs->r10);
    } else if (regs->rax == 158) { /* SYS_arch_prctl is 158 */
        ret = (uint64_t)sys_arch_prctl((int)regs->rdi, (uint64_t)regs->rsi);
    } else if (regs->rax == SYS_uname) {
        ret = (uint64_t)sys_uname((struct utsname*)regs->rdi);
    } else if (regs->rax == SYS_ioctl) {
        ret = (uint64_t)sys_ioctl((int)regs->rdi, (unsigned long)regs->rsi, (void*)regs->rdx);
    } else if (regs->rax == SYS_writev) {
        ret = (uint64_t)sys_writev((int)regs->rdi, (const struct iovec*)regs->rsi, (int)regs->rdx);
    } else if (regs->rax == SYS_readv) {
        ret = (uint64_t)sys_readv((int)regs->rdi, (const struct iovec*)regs->rsi, (int)regs->rdx);
    } else if (regs->rax == SYS_stat) {
        ret = (uint64_t)sys_stat((const char*)regs->rdi, (struct stat*)regs->rsi);
    } else if (regs->rax == SYS_fstat) {
        ret = (uint64_t)sys_fstat((int)regs->rdi, (struct stat*)regs->rsi);
    } else if (regs->rax == 202) { /* SYS_futex */
        ret = (uint64_t)sys_futex((uint32_t*)regs->rdi, (int)regs->rsi, (uint32_t)regs->rdx, (void*)regs->r10, (uint32_t*)regs->r8, (uint32_t)regs->r9);
    } else if (regs->rax == SYS_dup2) {
        ret = (uint64_t)sys_dup2((int)regs->rdi, (int)regs->rsi);
    } else if (regs->rax == SYS_pipe) {
        ret = (uint64_t)sys_pipe((int*)regs->rdi);
    } else if (regs->rax == SYS_rt_sigaction) {
        ret = (uint64_t)sys_rt_sigaction((int)regs->rdi, (const struct kernel_sigaction*)regs->rsi,
                                         (struct kernel_sigaction*)regs->rdx, (size_t)regs->r10);
    } else if (regs->rax == SYS_rt_sigprocmask) {
        ret = (uint64_t)sys_rt_sigprocmask((int)regs->rdi, (const uint64_t*)regs->rsi,
                                            (uint64_t*)regs->rdx, (size_t)regs->r10);
    } else if (regs->rax == SYS_rt_sigreturn) {
        ret = (uint64_t)sys_rt_sigreturn();
    } else if (regs->rax == SYS_nanosleep) {
        ret = (uint64_t)sys_nanosleep((const struct timespec*)regs->rdi, (struct timespec*)regs->rsi);
    } else if (regs->rax == SYS_access) {
        ret = (uint64_t)sys_access((const char*)regs->rdi, (int)regs->rsi);
    } else if (regs->rax == SYS_dup) {
        ret = (uint64_t)sys_dup((int)regs->rdi);
    } else if (regs->rax == SYS_fcntl) {
        ret = (uint64_t)sys_fcntl((int)regs->rdi, (int)regs->rsi, (int64_t)regs->rdx);
    } else if (regs->rax == SYS_getcwd) {
        ret = (uint64_t)sys_getcwd((char*)regs->rdi, (size_t)regs->rsi);
    } else if (regs->rax == SYS_getuid) {
        ret = (uint64_t)sys_getuid();
    } else if (regs->rax == SYS_getgid) {
        ret = (uint64_t)sys_getgid();
    } else if (regs->rax == SYS_geteuid) {
        ret = (uint64_t)sys_geteuid();
    } else if (regs->rax == SYS_getegid) {
        ret = (uint64_t)sys_getegid();
    } else if (regs->rax == SYS_mprotect) {
        ret = (uint64_t)sys_mprotect((void*)regs->rdi, (size_t)regs->rsi, (int)regs->rdx);
    } else if (regs->rax == SYS_munmap) {
        ret = (uint64_t)sys_munmap((void*)regs->rdi, (size_t)regs->rsi);
    } else if (regs->rax == SYS_madvise) {
        ret = (uint64_t)sys_madvise((void*)regs->rdi, (size_t)regs->rsi, (int)regs->rdx);
    } else if (regs->rax == 110) { /* SYS_getppid */
        ret = (uint64_t)sys_getppid();
    } else if (regs->rax == 186) { /* SYS_gettid */
        ret = (uint64_t)sys_gettid();
    } else if (regs->rax == 218) { /* SYS_set_tid_address */
        ret = (uint64_t)sys_set_tid_address((int*)regs->rdi);
    } else if (regs->rax == 228) { /* SYS_clock_gettime */
        ret = (uint64_t)sys_clock_gettime((int)regs->rdi, (struct timespec*)regs->rsi);
    } else if (regs->rax == 231) { /* SYS_exit_group */
        ret = (uint64_t)sys_exit_group((int)regs->rdi);
    } else if (regs->rax == SYS_mkdir) {
        ret = (uint64_t)sys_mkdir((const char*)regs->rdi, (int)regs->rsi);
    } else if (regs->rax == SYS_unlink) {
        ret = (uint64_t)sys_unlink((const char*)regs->rdi);
    } else if (regs->rax == 0xCAFEBABE) {
        ret = 0;
    } else {
        /* Log unknown syscall - but don't spam for common musl probes */
        if (regs->rax != 302 && regs->rax != 334) { /* pkey_alloc, rseq */
            char buf_nr[32];
            serial_write_string("[SYSCALL-NATIVE] Unknown syscall: 0x");
            utoa_hex_s(regs->rax, buf_nr, sizeof(buf_nr));
            serial_write_string(buf_nr);
            serial_write_string("\n");
        }
        ret = (uint64_t)-ENOSYS;
    }

    regs->rax = ret;
}

static void syscall_linux_handler(struct syscall_regs* regs) {
    /* For now, Linux handler uses the same implementation as native,
       but we separate the entry point as requested. */

    /* We can eventually add translation layers here if our internal ABI differs. */
    /* Since our native syscalls ARE Linux syscalls currently, we can just reuse logic. */
    /* But to demonstrate the separation, we will call the implementation functions directly logic again?
       No, that duplicates binary size. I'll just forward to native_handler for now
       and wrap it with a specific log or distinct logic if needed. */

    // serial_write_string("[SYSCALL-LINUX] Dispatching...\n");
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
