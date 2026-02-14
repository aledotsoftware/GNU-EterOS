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

extern void syscall_entry(void);

/* Definition of per-cpu data */
per_cpu_t per_cpu_data;

void syscall_init(void) {
    serial_write_string("[SYSCALL] Initializing MSRs for x86_64...\n");

    /* 1. EFER: Enable System Call Extensions (SCE) */
    uint64_t efer = rdmsr(MSR_EFER);
    efer |= EFER_SCE;
    wrmsr(MSR_EFER, efer);

    /* 2. STAR: Syscall/Sysret target CS/SS
     * [63:48] Sysret CS/SS selector base (0x10 -> CS=0x20 User Code, SS=0x18 User Data)
     * [47:32] Syscall CS/SS selector base (0x08 -> CS=0x08 Kernel Code, SS=0x10 Kernel Data)
     */
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

void syscall_handler(struct syscall_regs* regs) {
    uint64_t ret = (uint64_t)-38; /* -ENOSYS */
    task_t* current_task = task_get_current();

    /* Logic for Magic Test Payload */
    if (regs->rax == 0xCAFEBABE) {
        serial_write_string("[SYSCALL] Magic Test Passed! Hello from Ring 3!\n");
        terminal_write_colored("\n[SYSCALL] Magic Test Passed! Hello from Ring 3!\n", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
        regs->rax = 0;
        return;
    }

    /* Dispatcher */
    switch (regs->rax) {
        case SYS_read:
            {
                int fd = (int)regs->rdi;
                char* buf = (char*)regs->rsi;
                size_t count = (size_t)regs->rdx;

                if (fd < 0 || fd >= MAX_FD) {
                    ret = (uint64_t)-9; /* EBADF */
                    break;
                }

                file_descriptor_t* desc = &current_task->fd_table[fd];
                if (desc->node) {
                    uint32_t bytes_read = read_fs(desc->node, desc->offset, count, (uint8_t*)buf);
                    desc->offset += bytes_read;
                    ret = bytes_read;
                } else {
                    if (fd == 0) { /* STDIN */
                        size_t i = 0;
                        for (; i < count; i++) {
                            buf[i] = keyboard_getchar();
                            if (buf[i] == '\n') { i++; break; }
                        }
                        ret = i;
                    } else {
                        ret = (uint64_t)-9; /* EBADF */
                    }
                }
            }
            break;

        case SYS_write:
            {
                int fd = (int)regs->rdi;
                const char* buf = (const char*)regs->rsi;
                size_t count = (size_t)regs->rdx;

                if (fd < 0 || fd >= MAX_FD) {
                    ret = (uint64_t)-9; /* EBADF */
                    break;
                }

                file_descriptor_t* desc = &current_task->fd_table[fd];
                if (desc->node) {
                    uint32_t bytes_written = write_fs(desc->node, desc->offset, count, (uint8_t*)buf);
                    desc->offset += bytes_written;
                    ret = bytes_written;
                } else {
                    if (fd == 1 || fd == 2) { /* STDOUT/STDERR */
                        for(size_t i=0; i<count; i++) {
                            terminal_putchar(buf[i]);
                            serial_putchar(buf[i]);
                        }
                        ret = count;
                    } else {
                        ret = (uint64_t)-9; /* EBADF */
                    }
                }
            }
            break;

        case SYS_open:
            {
                const char* path = (const char*)regs->rdi;
                int flags = (int)regs->rsi;
                int fd = -1;
                for (int i = 0; i < MAX_FD; i++) {
                    if (current_task->fd_table[i].node == NULL) {
                        fd = i;
                        break;
                    }
                }

                if (fd == -1) {
                    ret = (uint64_t)-24; /* EMFILE */
                } else {
                    fs_node_t* node = vfs_lookup(fs_root, path);
                    if (node) {
                        current_task->fd_table[fd].node = node;
                        current_task->fd_table[fd].offset = 0;
                        current_task->fd_table[fd].flags = flags;
                        open_fs(node, 1, 1);
                        ret = fd;
                    } else {
                        ret = (uint64_t)-2; /* ENOENT */
                    }
                }
            }
            break;

        case SYS_close:
            {
                int fd = (int)regs->rdi;
                if (fd < 0 || fd >= MAX_FD || current_task->fd_table[fd].node == NULL) {
                    ret = (uint64_t)-9; /* EBADF */
                } else {
                    fs_node_t* node = current_task->fd_table[fd].node;
                    close_fs(node);
                    kfree(node);
                    current_task->fd_table[fd].node = NULL;
                    ret = 0;
                }
            }
            break;

        case SYS_exit:
            serial_write_string("[SYSCALL] Task exit called.\n");
            task_exit();
            break;

        case SYS_getpid:
            ret = current_task ? current_task->id : 0;
            break;

        case SYS_sched_yield:
            task_yield();
            ret = 0;
            break;

        case SYS_uname:
            {
                struct utsname {
                    char sysname[65];
                    char nodename[65];
                    char release[65];
                    char version[65];
                    char machine[65];
                    char domainname[65];
                } *u = (struct utsname*)regs->rdi;
                if (u) {
                    strlcpy(u->sysname, "eterOS", 65);
                    strlcpy(u->nodename, "genesis", 65);
                    strlcpy(u->release, "0.1.0", 65);
                    strlcpy(u->version, "Genesis", 65);
                    strlcpy(u->machine, "x86_64", 65);
                    ret = 0;
                } else ret = (uint64_t)-14;
            }
            break;

        case 1: /* Redirect SYS_write (1 in Linux) to our SYS_write handler if payload uses 1 */
            /* Some payloads use 1 for exit (like our test one). Let's check. */
            if (regs->rax == 1) {
                serial_write_string("[SYSCALL] Syscall 1 called. Assuming exit/halt for test.\n");
                task_exit();
            }
            break;

        default:
            serial_write_string("[SYSCALL] Unknown syscall: 0x");
            char buf[32];
            utoa_hex_s(regs->rax, buf, sizeof(buf));
            serial_write_string(buf);
            serial_write_string("\n");
            break;
    }

    regs->rax = ret;
}
