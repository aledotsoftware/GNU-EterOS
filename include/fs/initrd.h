#ifndef FS_INITRD_H
#define FS_INITRD_H

#include <types.h>

/**
 * Initializes the Initrd file system.
 *
 * @param start_addr Physical address where initrd is loaded.
 * @param size       Size of the initrd image in bytes.
 */
void initrd_init(uint64_t start_addr, uint32_t size);

/**
 * Internal file entry structure (on disk).
 */
typedef struct {
    char name[64];
    uint32_t size;
    uint32_t offset; /* Offset from start of initrd data */
} __attribute__((packed)) initrd_file_header_t;

/**
 * Reads a file from the Initrd.
 *
 * @param name Name of the file to find.
 * @param size Output pointer for file size (optional).
 * @return Pointer to the file data in memory, or NULL if not found.
 */
void* initrd_read_file(const char* name, uint32_t* size);

/**
 * Lists all files in the Initrd to the console.
 */
void initrd_list_files(void);

#endif
