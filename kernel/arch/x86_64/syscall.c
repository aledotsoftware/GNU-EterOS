/**
 * éterOS - x86_64 Syscall Handler
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 */

#include <syscall.h>
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

extern void syscall_entry(void);

/* Definition of per-cpu data */
per_cpu_t per_cpu_data;

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

    /* 5. GS_BASE: Point to Per-CPU data for swapgs */
    wrmsr(MSR_GS_BASE, (uint64_t)&per_cpu_data);
    wrmsr(MSR_KERNEL_GS_BASE, 0);

    /* Initialize stack for the current kernel thread (Task 0) */
    per_cpu_data.kernel_stack = 0x90000; 

    serial_write_string("[SYSCALL] x86_64 mechanism enabled.\n");
}

/* ========================================================================= */
/* Syscall Implementations                                                   */
/* ========================================================================= */

#define O_ACCMODE   00000003
#define O_RDONLY    00000000
#define O_WRONLY    00000001
#define O_RDWR      00000002

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

static int64_t sys_write(int fd, const void* buf, size_t count) {
    if (fd == 1 || fd == 2) {
        /* Stdout / Stderr -> Serial + VGA */
        const char* msg = (const char*)buf;
        for(size_t i=0; i<count; i++) {
            serial_putchar(msg[i]);
            terminal_putchar(msg[i]);
        }
        return count;
    }

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
    if (!node) return -ENOENT;

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

    close_fs(current->fd_table[fd].node);

    /* Free the node memory (vfs_lookup allocates it) */
    kfree(current->fd_table[fd].node);
    current->fd_table[fd].node = NULL;

    return 0;
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
    (void)sig; /* Signal handling mostly ignores sig type for now except termination */
    if (pid <= 0) return -EINVAL;

    /* Currently we only support termination-like behavior for kill */
    if (task_kill((uint32_t)pid) == 0) {
        return 0;
    }
    return -ESRCH;
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
    if (!buf) return -EFAULT;

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
    (void)fd; (void)request; (void)arg;
    /* Determine if it is a TTY */
    if (fd == 0 || fd == 1 || fd == 2) return 0; // Success for stdio
    return -ENOTTY;
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

void syscall_handler(struct syscall_regs* regs) {
    /* ULTRA DEBUG: Print immediately */
    /*
    char buf_nr[32];
    serial_write_string("[SYSCALL] ENTRY NR=0x");
    utoa_hex_s(regs->rax, buf_nr, sizeof(buf_nr));
    serial_write_string(buf_nr);
    serial_write_string("\n");
    */

    uint64_t ret = (uint64_t)-38; /* -ENOSYS */

    if (regs->rax == 0xCAFEBABE) {
        serial_write_string("[SYSCALL] Magic Number Detected!\n");
        ret = 0;
    } else if (regs->rax == SYS_read) {
        ret = (uint64_t)sys_read((int)regs->rdi, (void*)regs->rsi, (size_t)regs->rdx);
    } else if (regs->rax == SYS_write) {
        ret = (uint64_t)sys_write((int)regs->rdi, (const void*)regs->rsi, (size_t)regs->rdx);
    } else if (regs->rax == SYS_open) {
        ret = (uint64_t)sys_open((const char*)regs->rdi, (int)regs->rsi, (int)regs->rdx);
    } else if (regs->rax == SYS_close) {
        ret = (uint64_t)sys_close((int)regs->rdi);
    } else if (regs->rax == SYS_lseek) {
        ret = (uint64_t)sys_lseek((int)regs->rdi, (int64_t)regs->rsi, (int)regs->rdx);
    } else if (regs->rax == SYS_exit) {
        serial_write_string("[SYSCALL] Task exit called.\n");
        task_exit();
        __builtin_unreachable();
    } else if (regs->rax == SYS_getpid) {
        ret = (uint64_t)sys_getpid();
    } else if (regs->rax == SYS_kill) {
        ret = (uint64_t)sys_kill((int)regs->rdi, (int)regs->rsi);
    } else if (regs->rax == SYS_sched_yield) {
        task_yield();
        ret = 0;
    } else if (regs->rax == SYS_brk) {
        ret = (uint64_t)sys_brk((uint64_t)regs->rdi);
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
    }

    regs->rax = ret;
}
