#include <syscall.h>
#include <msr.h>
#include <cpu.h>
#include <serial.h>
#include <vga.h>
#include <string.h>

extern void syscall_entry(void);

per_cpu_t per_cpu_data;

void syscall_init(void) {
    serial_write_string("[SYSCALL] Initializing MSRs...\n");

    /* 1. EFER: Enable System Call Extensions (SCE) */
    uint64_t efer = rdmsr(MSR_EFER);
    efer |= EFER_SCE;
    wrmsr(MSR_EFER, efer);

    /* 2. STAR: Syscall/Sysret target CS/SS */
    /*
     * [63:48] Sysret CS/SS selector base (Ring 3 CS/SS)
     *   Sysret CS = (STAR[63:48] + 16) | 3 = 0x1B
     *   Sysret SS = (STAR[63:48] + 8)  | 3 = 0x13
     *
     * STAR[63:48] = 0x10 (Kernel Data Selector, Index 2).
     *   0x10 + 16 = 0x20 (User Code)
     *   0x10 + 8  = 0x18 (User Data)
     *
     * [47:32] Syscall CS/SS selector base (Ring 0 CS/SS)
     *   Syscall CS = STAR[47:32] = 0x08 (Kernel Code)
     *   Syscall SS = STAR[47:32] + 8 = 0x10 (Kernel Data)
     * STAR[47:32] = 0x08.
     */
    uint64_t star = 0;
    star |= (uint64_t)0x0008 << 32; /* Kernel CS Base */
    star |= (uint64_t)0x0010 << 48; /* User CS Base (offset calculation base) */
    wrmsr(MSR_STAR, star);

    /* 3. LSTAR: Target RIP for syscall */
    wrmsr(MSR_LSTAR, (uint64_t)syscall_entry);

    /* 4. SFMASK: RFLAGS mask (clear IF, TF, etc) */
    /* Disable Interrupts (IF=0x200) during syscall entry */
    wrmsr(MSR_SFMASK, 0x200);

    /* 5. GS_BASE: Point to Per-CPU data for swapgs */
    /* We are in kernel mode now, so GS_BASE points to per_cpu_data.
       When entering user mode, we will swapgs so GS_BASE becomes user TLS (or 0)
       and KERNEL_GS_BASE becomes &per_cpu_data. */
    wrmsr(MSR_GS_BASE, (uint64_t)&per_cpu_data);
    wrmsr(MSR_KERNEL_GS_BASE, 0);

    serial_write_string("[SYSCALL] Enabled.\n");
}

void syscall_handler(struct syscall_regs* regs) {
    char buf[32];
    uint64_t syscall_number = regs->rax;

    /* Log syscall 
    serial_write_string("[SYSCALL] #");
    utoa_hex_s(syscall_number, buf, sizeof(buf));
    serial_write_string(buf);
    serial_write_string("\n");
    */

    if (syscall_number == 0xCAFEBABE) {
        serial_write_string("[SYSCALL] Magic Test Passed! Hello from Ring 3!\n");
        terminal_write_colored("\n[SYSCALL] Magic Test Passed! Hello from Ring 3!\n", VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK);
    } else if (syscall_number == 1) { // SYS_write (or exit? Linux exit is 60. 1 is write in Linux x64)
        /* map 1 to write just for test, or stick to user code expecting something else? */
        /* original code had 1 as exit/halt */
        serial_write_string("[SYSCALL] Exit called. Halting.\n");
        for(;;) __asm__ volatile("hlt");
    } else {
        serial_write_string("[SYSCALL] Unknown syscall: 0x");
        utoa_hex_s(syscall_number, buf, sizeof(buf));
        serial_write_string(buf);
        serial_write_string("\n");
    }
}
