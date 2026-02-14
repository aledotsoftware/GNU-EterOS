#ifndef FS_INITRD_H
#define FS_INITRD_H

#include <types.h>
#include <fs/vfs.h>

/**
 * Initializes the Initrd file system and returns the root node.
 *
 * @param start_addr Physical address where initrd is loaded.
 * @param size       Size of the initrd image in bytes.
 * @return           Pointer to the root fs_node_t of the initrd.
 */
fs_node_t *initialise_initrd(uint64_t start_addr, uint32_t size);

/**
 * Internal file entry structure (on disk).
 */
typedef struct {
    char name[64];
    uint32_t size;
    uint32_t offset; /* Offset from start of initrd data */
} __attribute__((packed)) initrd_file_header_t;

/**
 * Legacy functions (may be deprecated or wrapped)
 */
void* initrd_read_file(const char* name, uint32_t* size);
void initrd_list_files(void);

#endif
