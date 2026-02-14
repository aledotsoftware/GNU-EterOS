#include "../../include/types.h"
#include "../../include/vmm.h"
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

    uint64_t entry_point = 0;

    /* Try to load ELF from Initrd */
    /* Note: We assume initrd is mounted at /initrd or root? */
    /* fs_root is the root of VFS. Usually Initrd is mounted at / or /initrd. */
    /* Let's try /test.elf first if initrd is root, or /initrd/test.elf if mounted. */
    /* In kernel/main.c usually initrd is root. */
    /* Let's try "test.elf" relative to root. */

    entry_point = elf_load_file("test.elf");

    if (entry_point == 0) {
        /* Try /test.elf */
        entry_point = elf_load_file("/test.elf");
    }

    if (entry_point != 0) {
        serial_write_string("[USER] ELF Loaded Successfully. Entry: 0x");
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
        /* PAGE_USER | PAGE_WRITE | PAGE_PRESENT */
        if (vmm_map_page((uint64_t)code_phys, user_code_virt, PAGE_PRESENT | PAGE_WRITE | PAGE_USER) < 0) {
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
        if (vmm_map_page((uint64_t)stack_phys, user_stack_virt - (i * PAGE_SIZE), PAGE_PRESENT | PAGE_WRITE | PAGE_USER) < 0) {
             serial_write_string("[USER] Failed to map stack page\n");
             return;
        }
    }

    /* Calculate Stack Top (grows down) */
    /* 0x80000000 is the TOP of the first page? No, usually base address. */
    /* If we map 0x80000000, valid range is 0x80000000 to 0x80000FFF. */
    /* Stack grows down, so top should be 0x80001000. */
    /* Wait, I mapped `user_stack_virt - (i*PAGE_SIZE)`. */
    /* If i=0: 0x80000000. i=1: 0x7FFFF000 (if PAGE_SIZE=4096). */
    /* So valid range: 0x7FFFF000 to 0x80000FFF. */
    /* Stack Top should be 0x80001000 (just past the last byte). */
    /* Or 0x80000FFF aligned? */
    /* x86 stack pointer points to last valid item? No, push decrements first. */
    /* So initial SP can be 0x80001000. */

    uint64_t user_stack_top = user_stack_virt + PAGE_SIZE;

    serial_write_string("[USER] Jumping to Ring 3...\n");
    enter_user_mode((void*)entry_point, (void*)user_stack_top);

    /* Should never return here */
    serial_write_string("[USER] Error: Returned from Ring 3 (unexpected)\n");
    for(;;) __asm__ volatile("hlt");
}
