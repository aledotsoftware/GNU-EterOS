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
    /* Load login.elf */
    entry_point = elf_load_file("login.elf", 0x200000000);

    if (entry_point == 0) {
        entry_point = elf_load_file("/login.elf", 0x200000000);
    }

    /* Fallback to sh.elf if login missing */
    if (entry_point == 0) {
         serial_write_string("[USER] login.elf not found, falling back to sh.elf\n");
         entry_point = elf_load_file("sh.elf", 0x200000000);
         if (entry_point == 0) entry_point = elf_load_file("/sh.elf", 0x200000000);
    }

    /* Fallback to test.elf if sh.elf missing */
    if (entry_point == 0) {
         serial_write_string("[USER] sh.elf not found, falling back to test.elf\n");
         entry_point = elf_load_file("test.elf", 0x200000000);
         if (entry_point == 0) entry_point = elf_load_file("/test.elf", 0x200000000);
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
    for (int i = 0; i < 2; i++) {
        void* stack_phys = pmm_alloc_page();
        if (!stack_phys) {
            serial_write_string("[USER] Failed to allocate stack page\n");
            return;
        }
        /* Stack: READ | WRITE | USER (No Exec) */
        if (hal_mem_map((uint64_t)stack_phys, user_stack_virt - (i * PAGE_SIZE), HAL_MEM_READ | HAL_MEM_WRITE | HAL_MEM_USER) < 0) {
             serial_write_string("[USER] Failed to map stack page\n");
             return;
        }
    }

    uint64_t user_stack_top = user_stack_virt + PAGE_SIZE;

    /* Push arguments to stack (argc, argv) */
    char* sp = (char*)user_stack_top;

    /* 1. Push strings */
    const char* argv0_str = "login";
    size_t len = strlen(argv0_str) + 1;
    sp -= len;
    memcpy(sp, argv0_str, len);
    char* argv0_ptr = sp;

    uintptr_t current_sp = (uintptr_t)sp;
    current_sp &= ~0xF; // Align down to 16 bytes
    sp = (char*)current_sp;

    /* 3. Push pointers */
    uint64_t* stack_ptr = (uint64_t*)sp;

    /* envp[0] = NULL */
    *(--stack_ptr) = 0;

    /* argv[1] = NULL */
    *(--stack_ptr) = 0;

    /* argv[0] = ptr */
    *(--stack_ptr) = (uint64_t)argv0_ptr;

    /* argc = 1 */
    *(--stack_ptr) = 1;

    serial_write_string("[USER] Jumping to Ring 3...\n");
    enter_user_mode((void*)entry_point, (void*)stack_ptr);

    /* Should never return here */
    serial_write_string("[USER] Error: Returned from Ring 3 (unexpected)\n");
    for(;;) __asm__ volatile("hlt");
}
