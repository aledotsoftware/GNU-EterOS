/**
 * éterOS - System Call Interface (x86_64)
 * Copyright (c) 2026 Tudex Networks. All rights reserved.
 *
 * Implementación de la tabla de syscalls y el dispatcher.
 */

#include "../include/syscall.h"
#include "../include/io.h"
#include "../include/serial.h"
#include "../include/string.h"
#include "../include/task.h"
#include "../include/vga.h"
#include "../include/keyboard.h"
#include <fs/vfs.h>
#include <mm.h>

/* ========================================================================= */
/* Constantes MSR                                                            */
/* ========================================================================= */
#define MSR_STAR        0xC0000081
#define MSR_LSTAR       0xC0000082
#define MSR_SFMASK      0xC0000084

/* ========================================================================= */
/* Prototipos de stubs en Assembly                                           */
/* ========================================================================= */
extern void syscall_entry(void);

/* ========================================================================= */
/* Implementación                                                            */
/* ========================================================================= */

void syscall_init(void) {
    /* 1. Configurar MSR_STAR */
    /* Bits 63:48 = Sysret CS/SS Base (0x10 -> CS=0x20 User Code, SS=0x18 User Data) */
    /* Bits 47:32 = Syscall CS/SS Base (0x08 -> CS=0x08 Kernel Code, SS=0x10 Kernel Data) */
    uint64_t star = ((uint64_t)0x0010 << 48) | ((uint64_t)0x0008 << 32);
    wrmsr(MSR_STAR, star);

    /* 2. Configurar MSR_LSTAR (Entry Point) */
    wrmsr(MSR_LSTAR, (uint64_t)syscall_entry);

    /* 3. Configurar MSR_SFMASK (RFLAGS Mask) */
    /* Deshabilitar interrupciones (IF 0x200) al entrar en syscall */
    wrmsr(MSR_SFMASK, 0x200);

    serial_write_string("[SYSCALL] Inicializado MSRs para SYSCALL/SYSRET\n");
}

/* Handler Genérico (Dispatcher) */
/* TODO: En el futuro, usar una tabla de punteros a función para O(1) dispatch */
void syscall_handler(struct syscall_regs* regs) {
    /* Por defecto, retornamos -ENOSYS (-38) */
    uint64_t ret = (uint64_t)-38;
    task_t* current_task = task_get_current();

    /* Debug: imprimir syscall number */
    /*
    char buf[32];
    serial_write_string("[SYSCALL] NR=");
    itoa_s(regs->rax, buf, sizeof(buf), 10);
    serial_write_string(buf);
    serial_write_string("\n");
    */

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
                    /* Fallback for stdin */
                    if (fd == 0) {
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
                    /* Fallback for stdout/stderr */
                    if (fd == 1 || fd == 2) {
                        for(size_t i=0; i<count; i++) {
                            terminal_putchar(buf[i]);
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
                /* int mode = (int)regs->rdx; */

                /* Find free FD */
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
                    current_task->fd_table[fd].offset = 0;
                    current_task->fd_table[fd].flags = 0;
                    ret = 0;
                }
            }
            break;

        case SYS_stat:
        case SYS_fstat:
        case SYS_lstat:
            /* VFS not implemented yet */
            ret = (uint64_t)-38; /* ENOSYS */
            break;

        case SYS_fork:
        case SYS_vfork:
        case SYS_clone:
            /* Process creation not fully implemented */
            ret = (uint64_t)-38; /* ENOSYS */
            break;

        case SYS_execve:
            /* Loading programs not implemented */
            ret = (uint64_t)-38; /* ENOSYS */
            break;

        case SYS_exit:
            /* sys_exit(status) */
            serial_write_string("[SYSCALL] sys_exit called.\n");
            task_exit();
            /* No retorna */
            break;

        case SYS_getpid:
            /* sys_getpid() */
            {
                task_t* current = task_get_current();
                if (current) ret = current->id;
                else ret = 0;
            }
            break;

        case SYS_sched_yield:
            task_yield();
            ret = 0;
            break;

        case SYS_sendto:
            /* sys_sendto(fd, buf, len, flags, addr, addrlen) */
            {
                extern void net_protocol_send(const char* data, size_t len);
                net_protocol_send((const char*)regs->rsi, (size_t)regs->rdx);
                ret = regs->rdx;
            }
            break;

        case SYS_recvfrom:
            /* sys_recvfrom(fd, buf, len, flags, addr, addrlen) */
            {
                extern size_t net_protocol_recv(char* buf, size_t max_len);
                ret = net_protocol_recv((char*)regs->rsi, (size_t)regs->rdx);
            }
            break;

        /* Essential POSIX Syscalls (Stubs) */
        case SYS_lseek:
            {
                int fd = (int)regs->rdi;
                uint64_t offset = (uint64_t)regs->rsi; /* off_t */
                int whence = (int)regs->rdx;

                if (fd < 0 || fd >= MAX_FD || current_task->fd_table[fd].node == NULL) {
                     ret = (uint64_t)-9; /* EBADF */
                     break;
                }

                file_descriptor_t* desc = &current_task->fd_table[fd];
                /* SEEK_SET 0, SEEK_CUR 1, SEEK_END 2 */
                if (whence == 0) desc->offset = (uint32_t)offset;
                else if (whence == 1) desc->offset += (uint32_t)offset;
                else if (whence == 2) desc->offset = desc->node->length + (uint32_t)offset;
                else { ret = (uint64_t)-22; break; } /* EINVAL */

                ret = desc->offset;
            }
            break;

        case SYS_poll:
        case SYS_mmap:
        case SYS_mprotect:
        case SYS_munmap:
        case SYS_brk:
        case SYS_rt_sigaction:
        case SYS_rt_sigprocmask:
        case SYS_rt_sigreturn:
        case SYS_ioctl:
        case SYS_pread64:
        case SYS_pwrite64:
        case SYS_readv:
        case SYS_writev:
        case SYS_access:
        case SYS_pipe:
        case SYS_select:
        case SYS_mremap:
        case SYS_msync:
        case SYS_mincore:
        case SYS_madvise:
        case SYS_shmget:
        case SYS_shmat:
        case SYS_shmctl:
        case SYS_dup:
        case SYS_dup2:
        case SYS_pause:
        case SYS_nanosleep:
        case SYS_getitimer:
        case SYS_alarm:
        case SYS_setitimer:
        case SYS_sendfile:
        case SYS_socket:
        case SYS_connect:
        case SYS_accept:
        case SYS_sendmsg:
        case SYS_recvmsg:
        case SYS_shutdown:
        case SYS_bind:
        case SYS_listen:
        case SYS_getsockname:
        case SYS_getpeername:
        case SYS_socketpair:
        case SYS_setsockopt:
        case SYS_getsockopt:
        case SYS_wait4:
            ret = (uint64_t)-38; /* ENOSYS for wait4 */
            break;

        case SYS_kill:
            {
                pid_t pid = (pid_t)regs->rdi;
                int sig = (int)regs->rsi;
                task_t* target = task_get_by_id(pid);
                if (target) {
                    if (sig > 0 && sig < 32) {
                        target->signal_pending |= (1 << (sig - 1));
                    }
                    ret = 0;
                } else {
                    ret = (uint64_t)-3; /* ESRCH */
                }
            }
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
                    strlcpy(u->domainname, "local", 65);
                    ret = 0;
                } else {
                    ret = (uint64_t)-14; /* EFAULT */
                }
            }
            break;

        case SYS_getuid:
        case SYS_getgid:
        case SYS_geteuid:
        case SYS_getegid:
            ret = 0; /* Always root for now */
            break;

        case SYS_semget:
        case SYS_semop:
        case SYS_semctl:
        case SYS_shmdt:
        case SYS_msgget:
        case SYS_msgsnd:
        case SYS_msgrcv:
        case SYS_msgctl:
        case SYS_fcntl:
        case SYS_flock:
        case SYS_fsync:
        case SYS_fdatasync:
        case SYS_truncate:
        case SYS_ftruncate:
        case SYS_getdents:
        case SYS_getcwd:
        case SYS_chdir:
        case SYS_fchdir:
        case SYS_rename:
        case SYS_mkdir:
        case SYS_rmdir:
        case SYS_creat:
        case SYS_link:
        case SYS_unlink:
        case SYS_symlink:
        case SYS_readlink:
        case SYS_chmod:
        case SYS_fchmod:
        case SYS_chown:
        case SYS_fchown:
        case SYS_lchown:
        case SYS_umask:
        case SYS_gettimeofday:
        case SYS_getrlimit:
        case SYS_getrusage:
        case SYS_sysinfo:
        case SYS_times:
            /* Not implemented yet, returns ENOSYS (-38) */
            // serial_write_string("[SYSCALL] Unimplemented syscall called\n");
            break;

        default:
            serial_write_string("[SYSCALL] Unknown syscall NR=");
            char buf[32];
            itoa_s(regs->rax, buf, sizeof(buf), 10);
            serial_write_string(buf);
            serial_write_string("\n");
            break;
    }

    regs->rax = ret;
}
