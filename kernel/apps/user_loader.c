#include "../../include/types.h"
#include "../../include/vmm.h"
#include "../../include/pmm.h"
#include "../../include/string.h"
#include "../../include/serial.h"
#include "../../include/task.h"

extern uint8_t user_payload_start[];
extern uint8_t user_payload_end[];
extern void enter_user_mode(void* entry, void* stack);

void user_loader_entry(void) {
    serial_write_string("[USER] Starting User Mode Test...\n");

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

    /* Allocate User Stack Page */
    void* stack_phys = pmm_alloc_page();
    if (!stack_phys) {
        serial_write_string("[USER] Failed to allocate stack page\n");
        return;
    }

    /* Map it to 0x200002000 (8GB + 8KB) */
    uint64_t user_stack_virt = 0x200002000;
    if (vmm_map_page((uint64_t)stack_phys, user_stack_virt, PAGE_PRESENT | PAGE_WRITE | PAGE_USER) < 0) {
         serial_write_string("[USER] Failed to map stack page\n");
         return;
    }

    /* Calculate Stack Top (grows down) */
    uint64_t user_stack_top = user_stack_virt + 4096;

    serial_write_string("[USER] Jumping to Ring 3...\n");
    enter_user_mode((void*)user_code_virt, (void*)user_stack_top);

    /* Should never return here */
    serial_write_string("[USER] Error: Returned from Ring 3 (unexpected)\n");
    for(;;) __asm__ volatile("hlt");
}
