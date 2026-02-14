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

void syscall_handler(struct syscall_regs* regs) {
    /* ULTRA DEBUG: Print immediately */
    char buf_nr[32];
    serial_write_string("[SYSCALL] ENTRY NR=0x");
    utoa_hex_s(regs->rax, buf_nr, sizeof(buf_nr));
    serial_write_string(buf_nr);
    serial_write_string("\n");

    uint64_t ret = (uint64_t)-38; /* -ENOSYS */

    if (regs->rax == 0xCAFEBABE) {
        serial_write_string("[SYSCALL] Magic Number Detected!\n");
        ret = 0;
    } else if (regs->rax == SYS_write) {
        /* Standard write handler */
        if (regs->rdi == 1 || regs->rdi == 2) {
             const char* msg = (const char*)regs->rsi;
             size_t len = (size_t)regs->rdx;
             for(size_t i=0; i<len; i++) {
                 serial_putchar(msg[i]);
                 terminal_putchar(msg[i]);
             }
             ret = len;
        }
    } else if (regs->rax == SYS_exit || regs->rax == 1) { /* Assume 1 is exit for our test */
        serial_write_string("[SYSCALL] Task exit called.\n");
        task_exit();
    }

    regs->rax = ret;
}
