#include "../../include/types.h"
#include "../../include/hal.h"
#include "../../include/pmm.h"
#include "../../include/string.h"
#include "../../include/serial.h"
#include "../../include/task.h"
#include "../../include/cpu.h"
#include "../../include/elf.h"
#include "../../include/fs/vfs.h"
#include "../../include/boot.h"

extern uint8_t user_payload_start[];
extern uint8_t user_payload_end[];
extern void enter_user_mode(void* entry, void* stack);
extern cpu_info_t* get_current_cpu(void);

static void setup_user_stdio(void) {
    fs_node_t* tty_node = vfs_lookup(fs_root, "/dev/tty");

    if (!tty_node) {
        serial_write_string("[USER] Warning: Failed to open /dev/tty\n");
        return;
    }

    task_t* current = task_get_current();
    current->fd_table[0].node = tty_node;
    current->fd_table[0].flags = 2;
    current->fd_table[0].offset = 0;
    tty_node->ref_count++;

    current->fd_table[1].node = tty_node;
    current->fd_table[1].flags = 2;
    current->fd_table[1].offset = 0;
    tty_node->ref_count++;

    current->fd_table[2].node = tty_node;
    current->fd_table[2].flags = 2;
    current->fd_table[2].offset = 0;
}

static void enter_loaded_user_program(uint64_t entry_point, const char* arg0) {
    uint64_t stack_top = 0x300000000 + PAGE_SIZE;
    for (int i = 1; i <= 4; i++) {
        void* stack_phys = pmm_alloc_page();
        if (!stack_phys) {
            serial_write_string("[USER] Error: Failed to allocate stack page\n");
            return;
        }
        uint64_t stack_page = stack_top - ((uint64_t)i * PAGE_SIZE);
        if (hal_mem_map((uint64_t)stack_phys, stack_page, HAL_MEM_READ | HAL_MEM_WRITE | HAL_MEM_USER) < 0) {
            serial_write_string("[USER] Error: Failed to map stack page\n");
            return;
        }
        memset((void*)stack_page, 0, PAGE_SIZE);
    }

    uint64_t rsp = stack_top;
    size_t arg0_len = strlen(arg0) + 1;
    rsp -= arg0_len;
    memcpy((void*)rsp, arg0, arg0_len);
    uint64_t arg0_user_ptr = rsp;
    uint64_t uargv[2];
    uint64_t uenvp[1];

    uargv[0] = arg0_user_ptr;
    uargv[1] = 0;
    uenvp[0] = 0;

    rsp &= ~0xFULL;

    /* Auxv terminator entry: { AT_NULL, 0 } */
    rsp -= 16;
    ((uint64_t*)rsp)[0] = 0;
    ((uint64_t*)rsp)[1] = 0;

    rsp -= sizeof(uenvp);
    memcpy((void*)rsp, uenvp, sizeof(uenvp));

    rsp -= sizeof(uargv);
    memcpy((void*)rsp, uargv, sizeof(uargv));

    /* argc */
    rsp -= 8;
    *(uint64_t*)rsp = 1;

    task_t* current = task_get_current();
    current->user_rsp = rsp;
    cpu_info_t* cpu = get_current_cpu();
    if (cpu) cpu->user_stack_scratch = rsp;

    serial_write_string("[USER] Jumping to Ring 3 (RIP=");
    char buf_rip[32]; utoa_hex_s(entry_point, buf_rip, sizeof(buf_rip)); serial_write_string(buf_rip);
    serial_write_string(", RSP=");
    char buf_rsp[32]; utoa_hex_s(rsp, buf_rsp, sizeof(buf_rsp)); serial_write_string(buf_rsp);
    serial_write_string(")...\n");

#ifndef __ETEROS_HOST_TEST__
    __asm__ volatile ("wrmsr" : : "c" (0xC0000100 /* MSR_FS_BASE */), "a" ((uint32_t)(current->fs_base & 0xFFFFFFFF)), "d" ((uint32_t)(current->fs_base >> 32)));
#endif

    enter_user_mode((void*)entry_point, (void*)rsp);
}

void user_loader_entry(void) {
    serial_write_string("[USER] Starting User Mode Loader...\n");

    setup_user_stdio();

    uint64_t entry_point = 0;
    const char* program_name = "sh";
    boot_info_t* boot_info = get_boot_info();
    int have_framebuffer = boot_info &&
                           boot_info->signature == 0x544F424B &&
                           boot_info->fb_addr != 0 &&
                           boot_info->fb_width != 0 &&
                           boot_info->fb_height != 0;

    /* Load login.elf as the primary entry point */
    entry_point = elf_load_file("login.elf", 0x200000000);
    if (entry_point == 0) entry_point = elf_load_file("/login.elf", 0x200000000);

    if (entry_point != 0) {
        program_name = "login";
        /* We pass the preferred shell based on framebuffer availability */
        if (have_framebuffer) {
            program_name = "login marea_shell.elf";
        } else {
            serial_write_string("[USER] No framebuffer available, using text shell fallback.\n");
            program_name = "login sh.elf";
        }
    } else {
        /* Fallback if login.elf isn't found for some reason */
        if (have_framebuffer) {
            entry_point = elf_load_file("marea_shell.elf", 0x200000000);
            if (entry_point == 0) entry_point = elf_load_file("/marea_shell.elf", 0x200000000);
            if (entry_point != 0) program_name = "marea_shell";
        }

        if (entry_point == 0) {
            entry_point = elf_load_file("sh.elf", 0x200000000);
            if (entry_point == 0) entry_point = elf_load_file("/sh.elf", 0x200000000);
            if (entry_point != 0) program_name = "sh";
        }
    }

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

    enter_loaded_user_program(entry_point, program_name);

    /* Should never return here */
    serial_write_string("[USER] Error: Returned from Ring 3 (unexpected)\n");
    for(;;) hal_cpu_halt();
}
