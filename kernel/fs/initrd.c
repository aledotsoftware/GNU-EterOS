#include <fs/initrd.h>
#include <string.h>
#include <hal.h>

#define INITRD_MAGIC "INIT"

typedef struct {
    char magic[4];
    uint32_t count;
} __attribute__((packed)) initrd_header_t;

/* Global state */
static uint8_t* initrd_start = NULL;
static uint32_t file_count = 0;
static initrd_file_header_t* file_headers = NULL;

void initrd_init(uint64_t start_addr, uint32_t size) {
    if (start_addr == 0 || size == 0) {
        hal_console_write("[INITRD] No Initrd detected (Address 0 or Size 0).\n");
        return;
    }

    initrd_start = (uint8_t*)start_addr;
    /* In 64-bit mode, pointer arithmetic works fine */

    /* Verify Magic */
    initrd_header_t* header = (initrd_header_t*)initrd_start;
    if (memcmp(header->magic, INITRD_MAGIC, 4) != 0) {
        hal_console_write("[INITRD] Error: Invalid Magic.\n");
        /* Reset state to avoid crashes */
        initrd_start = NULL;
        return;
    }

    file_count = header->count;
    /* File headers start after the main header (8 bytes) */
    file_headers = (initrd_file_header_t*)(initrd_start + sizeof(initrd_header_t));

    hal_console_write("[INITRD] Initialized at 0x");
    char addr_buf[32];
    utoa_hex_s(start_addr, addr_buf, sizeof(addr_buf));
    hal_console_write(addr_buf);

    hal_console_write(". Found ");
    char count_buf[16];
    itoa_s(file_count, count_buf, sizeof(count_buf), 10);
    hal_console_write(count_buf);
    hal_console_write(" files.\n");

    initrd_list_files();
}

void* initrd_read_file(const char* name, uint32_t* size) {
    if (!initrd_start) return NULL;

    for (uint32_t i = 0; i < file_count; i++) {
        /* Ensure null termination safety implicitly by strcmp limits */
        /* Note: initrd_file_header_t.name is 64 chars, might not be null terminated if exactly 64 chars?
           mkinitrd.py ensures null termination or truncation.
           But we should be careful. strcmp stops at null. */

        if (strcmp(file_headers[i].name, name) == 0) {
            if (size) *size = file_headers[i].size;
            /* Offset in file_header is relative to initrd_start */
            return (void*)(initrd_start + file_headers[i].offset);
        }
    }
    return NULL;
}

void initrd_list_files(void) {
    if (!initrd_start) return;

    hal_console_write("  [INITRD] Content:\n");
    for (uint32_t i = 0; i < file_count; i++) {
        hal_console_write("    - ");
        hal_console_write(file_headers[i].name);
        hal_console_write(" (");

        char size_buf[16];
        itoa_s(file_headers[i].size, size_buf, sizeof(size_buf), 10);
        hal_console_write(size_buf);
        hal_console_write(" bytes)\n");
    }
}
