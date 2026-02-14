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
            /* sys_read(fd, buf, count) */
            if (regs->rdi == 0) { /* STDIN */
                char* buf = (char*)regs->rsi;
                size_t count = (size_t)regs->rdx;
                size_t i = 0;
                /* Simple implementation: read from keyboard buffer */
                for (; i < count; i++) {
                    buf[i] = keyboard_getchar();
                    /* Echo? Maybe not here. */
                    if (buf[i] == '\n') { i++; break; } /* Line buffered-ish */
                }
                ret = i;
            } else {
                ret = (uint64_t)-9; /* EBADF */
            }
            break;

        case SYS_write:
            /* sys_write(fd, buf, count) */
            /* Implementación temporal directa a consola */
            if (regs->rdi == 1 || regs->rdi == 2) {
                const char* s = (const char*)regs->rsi;
                size_t len = (size_t)regs->rdx;
                /* Validar puntero s (TODO) */
                for(size_t i=0; i<len; i++) {
                    terminal_putchar(s[i]);
                }
                ret = len;
            } else {
                ret = (uint64_t)-9; /* EBADF (-9) */
            }
            break;

        case SYS_open:
        case SYS_close:
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
