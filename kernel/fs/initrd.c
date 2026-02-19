#include <fs/initrd.h>
#include <fs/vfs.h>
#include <fs/devfs.h>
#include <fs/procfs.h>
#include <string.h>
#include <hal.h>
#include <mm.h>

#define INITRD_MAGIC "INIT"

typedef struct {
    char magic[4];
    uint32_t count;
} __attribute__((packed)) initrd_header_t;

/* Global state */
static uint8_t* initrd_start = NULL;
static uint32_t file_count = 0;
static initrd_file_header_t* file_headers = NULL;
static fs_node_t *initrd_root = NULL;             /* The root directory node */

static fs_node_t *devfs_root_node = NULL;
static fs_node_t *procfs_root_node = NULL;

static struct dirent dirent; /* Static dirent for readdir */

uint32_t initrd_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    initrd_file_header_t header = file_headers[node->inode];
    if (offset > header.size)
        return 0;
    if (offset + size > header.size)
        size = header.size - offset;

    memcpy(buffer, (uint8_t*)(initrd_start + header.offset + offset), size);
    return size;
}

struct dirent *initrd_readdir(fs_node_t *node, uint32_t index) {
    if (node != initrd_root)
        return 0;

    /* Virtual entries */
    if (index == 0) {
        strlcpy(dirent.name, "dev", sizeof(dirent.name));
        dirent.inode = 0;
        return &dirent;
    }
    if (index == 1) {
        strlcpy(dirent.name, "proc", sizeof(dirent.name));
        dirent.inode = 0;
        return &dirent;
    }
    if (index == 2) {
        strlcpy(dirent.name, "sys", sizeof(dirent.name));
        dirent.inode = 0;
        return &dirent;
    }

    /* Initrd files (offset by 3) */
    uint32_t file_index = index - 3;

    if (file_index >= file_count)
        return 0;

    strlcpy(dirent.name, file_headers[file_index].name, sizeof(dirent.name));
    dirent.inode = file_index;
    return &dirent;
}

fs_node_t *initrd_finddir(fs_node_t *node, char *name) {
    if (node != initrd_root)
        return 0;

    /* Check for virtual directories */
    if (strcmp(name, "dev") == 0) {
        if (devfs_root_node) {
            fs_node_t* copy = (fs_node_t*)kmalloc(sizeof(fs_node_t));
            if (copy) {
                memcpy(copy, devfs_root_node, sizeof(fs_node_t));
                copy->ref_count = 1;
            }
            return copy;
        }
        return 0;
    }
    if (strcmp(name, "proc") == 0) {
        if (procfs_root_node) {
            fs_node_t* copy = (fs_node_t*)kmalloc(sizeof(fs_node_t));
            if (copy) {
                memcpy(copy, procfs_root_node, sizeof(fs_node_t));
                copy->ref_count = 1;
            }
            return copy;
        }
        return 0;
    }
    if (strcmp(name, "sys") == 0) {
        /* Placeholder for sysfs */
        fs_node_t* fnode = (fs_node_t*)kmalloc(sizeof(fs_node_t));
        if (!fnode) return 0;
        memset(fnode, 0, sizeof(fs_node_t));
        fnode->ref_count = 1;
        strlcpy(fnode->name, "sys", sizeof(fnode->name));
        fnode->flags = FS_DIRECTORY;
        return fnode;
    }

    for (uint32_t i = 0; i < file_count; i++) {
        if (strcmp(name, file_headers[i].name) == 0) {
             fs_node_t *fnode = (fs_node_t*)kmalloc(sizeof(fs_node_t));
             if (!fnode) return 0;
             memset(fnode, 0, sizeof(fs_node_t));
             fnode->ref_count = 1;
             strlcpy(fnode->name, file_headers[i].name, sizeof(fnode->name));
             fnode->inode = i;
             fnode->flags = FS_FILE;
             fnode->read = &initrd_read;
             fnode->write = 0;
             fnode->open = 0;
             fnode->close = 0;
             fnode->readdir = 0;
             fnode->finddir = 0;
             fnode->length = file_headers[i].size;
             return fnode;
        }
    }
    return 0;
}

fs_node_t *initialise_initrd(uint64_t start_addr, uint32_t size) {
    if (start_addr == 0 || size == 0) {
        hal_console_write("[INITRD] No Initrd detected (Address 0 or Size 0).\n");
        return NULL;
    }

    initrd_start = (uint8_t*)start_addr;

    /* Verify Magic */
    initrd_header_t* header = (initrd_header_t*)initrd_start;
    if (memcmp(header->magic, INITRD_MAGIC, 4) != 0) {
        hal_console_write("[INITRD] Error: Invalid Magic.\n");
        initrd_start = NULL;
        return NULL;
    }

    file_count = header->count;
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

    /* Initialize Virtual Filesystems */
    devfs_root_node = devfs_init();
    procfs_root_node = procfs_init();

    initrd_root = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!initrd_root) {
        hal_console_write("[INITRD] Error: Failed to allocate root node.\n");
        return NULL;
    }
    memset(initrd_root, 0, sizeof(fs_node_t));
    initrd_root->ref_count = 1;
    strlcpy(initrd_root->name, "initrd", sizeof(initrd_root->name));
    initrd_root->mask = initrd_root->uid = initrd_root->gid = initrd_root->inode = initrd_root->length = 0;
    initrd_root->flags = FS_DIRECTORY;
    initrd_root->read = 0;
    initrd_root->write = 0;
    initrd_root->open = 0;
    initrd_root->close = 0;
    initrd_root->readdir = &initrd_readdir;
    initrd_root->finddir = &initrd_finddir;
    initrd_root->ptr = 0;
    initrd_root->impl = 0;

    return initrd_root;
}

/* Legacy helpers wrappers */

void* initrd_read_file(const char* name, uint32_t* size) {
    if (!initrd_start) return NULL;
    for (uint32_t i = 0; i < file_count; i++) {
        if (strcmp(file_headers[i].name, name) == 0) {
            if (size) *size = file_headers[i].size;
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
