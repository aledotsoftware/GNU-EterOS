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
    (void)flags; (void)mode;
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

    open_fs(node, 0, 0); /* flags not fully supported yet */

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
        /* task_exit() never returns — it marks the task DEAD and context-switches away.
         * We must NOT return from syscall_handler after this, or sysret will
         * jump back to the user RIP which is no longer valid. */
        task_exit();
        /* Never reached */
        __builtin_unreachable();
    } else if (regs->rax == SYS_getpid) {
        ret = (uint64_t)sys_getpid();
    } else if (regs->rax == SYS_kill) {
        ret = (uint64_t)sys_kill((int)regs->rdi, (int)regs->rsi);
    } else if (regs->rax == SYS_sched_yield) {
        task_yield();
        ret = 0;
    }

    regs->rax = ret;
}
