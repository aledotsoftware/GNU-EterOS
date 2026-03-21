#include "../../include/types.h"
#include "../../include/hal.h"
#include "../../include/pmm.h"
#include "../../include/string.h"
#include "../../include/serial.h"
#include "../../include/task.h"
#include "../../include/elf.h"
#include "../../include/fs/vfs.h"

extern uint8_t user_payload_start[];
extern uint8_t user_payload_end[];
extern void enter_user_mode(void* entry, void* stack);

void user_loader_entry(void) {
    serial_write_string("[USER] Starting User Mode Loader...\n");

    /* Setup Stdin/Stdout/Stderr to TTY */
    fs_node_t* tty_node = vfs_lookup(fs_root, "/dev/tty");

    if (tty_node) {
        task_t* current = task_get_current();
        /* FD 0: Stdin */
        current->fd_table[0].node = tty_node;
        current->fd_table[0].flags = 2; /* O_RDWR */
        current->fd_table[0].offset = 0;
        tty_node->ref_count++;

        /* FD 1: Stdout */
        current->fd_table[1].node = tty_node;
        current->fd_table[1].flags = 2;
        current->fd_table[1].offset = 0;
        tty_node->ref_count++;

        /* FD 2: Stderr */
        current->fd_table[2].node = tty_node;
        current->fd_table[2].flags = 2;
        current->fd_table[2].offset = 0;
        tty_node->ref_count++;

    } else {
        serial_write_string("[USER] Warning: Failed to open /dev/tty\n");
    }

    uint64_t entry_point = 0;

    /* Try to load ELF from Initrd */
    /* Primary: Load Userspace Shell (Terminal Mode requested by user) */
    entry_point = elf_load_file("sh.elf", 0x200000000);
    if (entry_point == 0) entry_point = elf_load_file("/sh.elf", 0x200000000);
    if (entry_point == 0) entry_point = elf_load_file("marea_shell.elf", 0x200000000);
    if (entry_point == 0) entry_point = elf_load_file("/marea_shell.elf", 0x200000000);

    if (entry_point == 0) {
        serial_write_string("[USER] Warning: No user shell binary found in initrd. Automatic user-mode shell disabled.\n");
        serial_write_string("[USER] You can use the kernel shell to launch binaries manually.\n");
        while(1) { task_yield(); hal_cpu_halt(); }
    }

    if (entry_point != 0) {
        serial_write_string("[USER] ELF Loaded Successfully. Entry: ");
        char buf[32];
        utoa_hex_s(entry_point, buf, sizeof(buf));
        serial_write_string(buf);
        serial_write_string("\n");
    } else {
        serial_write_string("[USER] ELF load failed or not found. Falling back to raw payload.\n");

        /* Fallback: Raw Payload logic */
        /* Allocate User Code Page */
        void* code_phys = pmm_alloc_page();
        if (!code_phys) {
            serial_write_string("[USER] Failed to allocate code page\n");
            return;
        }

        /* Map it to 0x200000000 (8GB, well above bootloader mapping) */
        uint64_t user_code_virt = 0x200000000;
        /* USER | WRITE | READ | EXEC */
        if (hal_mem_map((uint64_t)code_phys, user_code_virt, HAL_MEM_READ | HAL_MEM_WRITE | HAL_MEM_USER | HAL_MEM_EXEC) < 0) {
             serial_write_string("[USER] Failed to map code page\n");
             return;
        }

        /* Copy payload */
        uint64_t payload_size = (uint64_t)(user_payload_end - user_payload_start);
        memcpy((void*)user_code_virt, user_payload_start, payload_size);

        entry_point = user_code_virt;
    }

    /* Allocate User Stack Page */
    /* Map it to 0x300000000 (12GB) to avoid conflict with Bootloader 4GB Identity Map */

    uint64_t user_stack_virt = 0x300000000;
    for (int i = 0; i < 4; i++) { /* 16KB stack */
        void* stack_phys = pmm_alloc_page();
        if (!stack_phys) {
            serial_write_string("[USER] Error: Failed to allocate stack page\n");
            return;
        }
        /* Stack: READ | WRITE | USER (No Exec) */
        if (hal_mem_map((uint64_t)stack_phys, user_stack_virt - (i * PAGE_SIZE), HAL_MEM_READ | HAL_MEM_WRITE | HAL_MEM_USER) < 0) {
             serial_write_string("[USER] Error: Failed to map stack page\n");
             return;
        }
    }

    /* System V ABI requires 16-byte alignment for main() entry */
    /* The stack should look like this (from high to low):
     * [envp[n]] = NULL
     * ...
     * [envp[0]] = ptr
     * [argv[m]] = NULL
     * ...
     * [argv[0]] = ptr
     * [argc]    = value  <-- RSP points here
     */

    uint64_t* sp = (uint64_t*)(user_stack_virt + PAGE_SIZE);

    /* 1. Push environment strings (empty for now) */
    /* 2. Push argument strings */
    const char* arg0 = "sh";
    size_t arg0_len = strlen(arg0) + 1;
    char* arg0_user_ptr = (char*)((uintptr_t)sp - arg0_len);
    memcpy(arg0_user_ptr, arg0, arg0_len);
    sp = (uint64_t*)((uintptr_t)arg0_user_ptr & ~7); /* Align for pointers */

    /* 3. Construct the pointer arrays */
    /* Push NULL for envp */
    *(--sp) = 0; 
    
    /* Push NULL for argv */
    *(--sp) = 0;
    /* Push argv[0] */
    *(--sp) = (uint64_t)arg0_user_ptr;

    /* Push argc */
    uint64_t argc = 1;
    *(--sp) = argc;

    /* Ensure RSP is 16-byte aligned before calling main (crt0 does pop rdi first) */
    /* After 'pop rdi', RSP must be 16-byte aligned. */
    /* So before pop, RSP should be 8-byte aligned but NOT 16-byte aligned? 
     * Actually, System V says: The stack is 16-byte aligned BEFORE the 'call'.
     * 'call' pushes RIP (8 bytes), so main() sees RSP+8 aligned.
     * Our crt0 starts at _start, pops argc (8 bytes). 
     * So we need (RSP - 8) to be 16-byte aligned. */
    
    uintptr_t final_sp = (uintptr_t)sp;
    if ((final_sp % 16) == 0) {
        /* If it's 16-aligned, we subtract 8 to make it (16n - 8) */
        /* When crt0 pops argc, it becomes 16-aligned. */
        sp--; 
        *sp = argc; /* Re-push argc at new position */
    }

    serial_write_string("[USER] Jumping to Ring 3 (RIP=");
    char buf_rip[32]; utoa_hex_s(entry_point, buf_rip, sizeof(buf_rip)); serial_write_string(buf_rip);
    serial_write_string(", RSP=");
    char buf_rsp[32]; utoa_hex_s((uint64_t)sp, buf_rsp, sizeof(buf_rsp)); serial_write_string(buf_rsp);
    serial_write_string(")...\n");

    enter_user_mode((void*)entry_point, (void*)sp);

    /* Should never return here */
    serial_write_string("[USER] Error: Returned from Ring 3 (unexpected)\n");
    for(;;) __asm__ volatile("hlt");
}
