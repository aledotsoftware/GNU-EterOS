#include <fs/initrd.h>
#include <errno.h>
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

typedef struct initrd_dir {
    char name[128];
    uint32_t inode;
    struct initrd_dir *next;
} initrd_dir_t;

/* Global state */
static uint8_t* initrd_start = NULL;
static uint32_t initrd_image_size = 0;
static uint32_t file_count = 0;
static initrd_file_header_t* file_headers = NULL;
static fs_node_t *initrd_root = NULL;             /* The root directory node */

static initrd_dir_t *virtual_dirs = NULL;
static uint32_t virtual_dirs_count = 0;

ssize_t initrd_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
int initrd_readdir(fs_node_t *node, uint32_t index, struct dirent *entry);
int initrd_create(fs_node_t *parent, char *name, uint16_t permission);
int initrd_mkdir(fs_node_t *parent, char *name, uint16_t permission);
fs_node_t *initrd_finddir(fs_node_t *node, char *name);

static int initrd_is_root_handle(fs_node_t *node) {
    if (!node) return 0;
    return ((node->flags & 0x7) == FS_DIRECTORY) &&
           node->inode == 0 &&
           node->impl == 0 &&
           node->readdir == &initrd_readdir &&
           node->finddir == &initrd_finddir;
}

/* Virtual Dir Helper */
static initrd_dir_t* find_virtual_dir(const char* name) {
    initrd_dir_t* current = virtual_dirs;
    while (current) {
        if (strcmp(current->name, (char*)name) == 0) return current;
        current = current->next;
    }
    return NULL;
}

static const char* initrd_dir_prefix(fs_node_t *node) {
    if (!node) return NULL;
    if ((node->flags & 0x7) != FS_DIRECTORY) return NULL;
    if (initrd_is_root_handle(node)) return "";
    if (node->readdir == &initrd_readdir && node->finddir == &initrd_finddir && node->ptr) {
        return (const char*)node->ptr;
    }
    return NULL;
}

static int initrd_extract_child(const char* full, const char* prefix, char* out, size_t out_sz) {
    const char* rem = full;
    const char* slash;
    size_t plen;
    size_t len;

    if (!full || !prefix || !out || out_sz == 0) return 0;

    plen = strlen(prefix);
    if (plen > 0) {
        if (strncmp(full, prefix, plen) != 0) return 0;
        if (full[plen] != '/') return 0;
        rem = full + plen + 1;
    }

    if (*rem == '\0') return 0;
    slash = strchr(rem, '/');
    len = slash ? (size_t)(slash - rem) : strlen(rem);
    if (len == 0 || len >= out_sz) return 0;

    memcpy(out, rem, len);
    out[len] = '\0';
    return 1;
}

static fs_node_t* initrd_make_dir_node(const char* name, const char* full_path) {
    fs_node_t* fnode = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!fnode) return 0;
    memset(fnode, 0, sizeof(fs_node_t));
    fnode->ref_count = 1;
    strlcpy(fnode->name, name, sizeof(fnode->name));
    fnode->mask = 0755;
    fnode->flags = FS_DIRECTORY;
    fnode->readdir = &initrd_readdir;
    fnode->finddir = &initrd_finddir;
    fnode->mkdir = &initrd_mkdir;
    fnode->create = &initrd_create;
    if (full_path && full_path[0]) {
        size_t n = strlen(full_path) + 1;
        char* p = (char*)kmalloc(n);
        if (!p) {
            kfree(fnode);
            return 0;
        }
        memcpy(p, full_path, n);
        fnode->ptr = (fs_node_t*)p;
    }
    return fnode;
}

ssize_t initrd_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    initrd_file_header_t header = file_headers[node->inode];
    if (offset > header.size)
        return 0;
    if (offset + size > header.size)
        size = header.size - offset;

    /* Security Check: Ensure read does not exceed physical initrd size */
    if (header.offset >= initrd_image_size)
        return 0;

    /* Check for overflow */
    uint64_t end_pos = (uint64_t)header.offset + offset + size;

    if (end_pos > initrd_image_size) {
        if ((uint64_t)header.offset + offset >= initrd_image_size) {
             return 0;
        }
        size = (uint32_t)(initrd_image_size - (header.offset + offset));
    }

    memcpy(buffer, (uint8_t*)(initrd_start + header.offset + offset), size);
    return size;
}

int initrd_readdir(fs_node_t *node, uint32_t index, struct dirent *entry) {
    const char* prefix = initrd_dir_prefix(node);
    char child[128];
    uint32_t seen = 0;
    if (!prefix) return -1;

    /* Virtual dirs first */
    for (initrd_dir_t* cur = virtual_dirs; cur; cur = cur->next) {
        if (!initrd_extract_child(cur->name, prefix, child, sizeof(child))) continue;
        int duplicate = 0;
        for (initrd_dir_t* p = virtual_dirs; p != cur; p = p->next) {
            char prev_child[128];
            if (initrd_extract_child(p->name, prefix, prev_child, sizeof(prev_child)) &&
                strcmp(prev_child, child) == 0) {
                duplicate = 1;
                break;
            }
        }
        if (!duplicate) {
            for (uint32_t i = 0; i < file_count; i++) {
                char prev_child[128];
                size_t name_len = strnlen(file_headers[i].name, sizeof(file_headers[i].name));
                char safe_name[65];
                if (name_len > 64) name_len = 64;
                memcpy(safe_name, file_headers[i].name, name_len);
                safe_name[name_len] = '\0';
                if (initrd_extract_child(safe_name, prefix, prev_child, sizeof(prev_child)) &&
                    strcmp(prev_child, child) == 0) {
                    duplicate = 1;
                    break;
                }
            }
        }
        if (duplicate) continue;
        if (seen == index) {
            strlcpy(entry->name, child, sizeof(entry->name));
            entry->inode = cur->inode;
            return 0;
        }
        seen++;
    }

    for (uint32_t i = 0; i < file_count; i++) {
        size_t name_len = strnlen(file_headers[i].name, sizeof(file_headers[i].name));
        char safe_name[65];
        if (name_len > 64) name_len = 64;
        memcpy(safe_name, file_headers[i].name, name_len);
        safe_name[name_len] = '\0';

        if (!initrd_extract_child(safe_name, prefix, child, sizeof(child))) continue;
        int duplicate = 0;
        for (uint32_t j = 0; j < i; j++) {
            char prev_child[128];
            size_t prev_name_len = strnlen(file_headers[j].name, sizeof(file_headers[j].name));
            char prev_safe_name[65];
            if (prev_name_len > 64) prev_name_len = 64;
            memcpy(prev_safe_name, file_headers[j].name, prev_name_len);
            prev_safe_name[prev_name_len] = '\0';

            if (initrd_extract_child(prev_safe_name, prefix, prev_child, sizeof(prev_child)) &&
                strcmp(prev_child, child) == 0) {
                duplicate = 1;
                break;
            }
        }
        if (!duplicate) {
            for (initrd_dir_t* p = virtual_dirs; p; p = p->next) {
                char prev_child[128];
                if (initrd_extract_child(p->name, prefix, prev_child, sizeof(prev_child)) &&
                    strcmp(prev_child, child) == 0) {
                    duplicate = 1;
                    break;
                }
            }
        }
        if (duplicate) continue;
        if (seen == index) {
            strlcpy(entry->name, child, sizeof(entry->name));
            entry->inode = i;
            return 0;
        }
        seen++;
    }

    return 1; /* EOF */
}

int initrd_create(fs_node_t *parent, char *name, uint16_t permission) {
    (void)parent; (void)name; (void)permission;
    return -EROFS; // Initrd is read-only
}

int initrd_mkdir(fs_node_t *parent, char *name, uint16_t permission) {
    (void)permission;

    char target[256];
    const char* prefix = initrd_dir_prefix(parent);

    if (prefix && prefix[0] != '\0') {
        strlcpy(target, prefix, sizeof(target));
        strlcat(target, "/", sizeof(target));
        strlcat(target, name, sizeof(target));
    } else {
        strlcpy(target, name, sizeof(target));
    }

    if (find_virtual_dir(target)) return -1; /* Already exists */

    /* Check conflicts with real files */
    for (uint32_t i = 0; i < file_count; i++) {
        size_t name_len = strnlen(file_headers[i].name, sizeof(file_headers[i].name));
        if (strncmp(target, file_headers[i].name, name_len) == 0 && target[name_len] == '\0') return -1;
    }

    initrd_dir_t *new_dir = (initrd_dir_t*)kmalloc(sizeof(initrd_dir_t));
    if (!new_dir) return -2;

    strlcpy(new_dir->name, target, sizeof(new_dir->name));
    /* Assign generic inode high up to avoid collision with file indices */
    new_dir->inode = 0xF0000000 + virtual_dirs_count;
    new_dir->next = virtual_dirs;
    virtual_dirs = new_dir;
    virtual_dirs_count++;

    return 0;
}

fs_node_t *initrd_finddir(fs_node_t *node, char *name) {
    const char* prefix = initrd_dir_prefix(node);
    char target[256];
    if (!prefix || !name || !*name) return 0;

    if (prefix[0] == '\0') {
        strlcpy(target, name, sizeof(target));
    } else {
        strlcpy(target, prefix, sizeof(target));
        strlcat(target, "/", sizeof(target));
        strlcat(target, name, sizeof(target));
    }

    /* Check for virtual directories */
    initrd_dir_t* vdir = find_virtual_dir(target);
    if (vdir) {
        fs_node_t* fnode = initrd_make_dir_node(name, target);
        if (!fnode) return 0;
        fnode->inode = vdir->inode;
        return fnode;
    }

    for (uint32_t i = 0; i < file_count; i++) {
        size_t name_len = strnlen(file_headers[i].name, sizeof(file_headers[i].name));
        if (name_len > 64) name_len = 64; /* Cap name length for security */

        char safe_name[65];
        memcpy(safe_name, file_headers[i].name, name_len);
        safe_name[name_len] = '\0';

        if (strncmp(target, safe_name, name_len) == 0 && target[name_len] == '\0') {
             fs_node_t *fnode = (fs_node_t*)kmalloc(sizeof(fs_node_t));
             if (!fnode) return 0;
             memset(fnode, 0, sizeof(fs_node_t));
             fnode->ref_count = 1;

             /* Securely copy name */
             if (name_len >= sizeof(fnode->name)) name_len = sizeof(fnode->name) - 1;
             memcpy(fnode->name, safe_name, name_len);
             fnode->name[name_len] = '\0';

             fnode->inode = i;
             fnode->mask = 0755; /* Files typically rwxr-xr-x (executable for elf) */
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

    {
        size_t tlen = strnlen(target, sizeof(target));
        for (uint32_t i = 0; i < file_count; i++) {
            size_t name_len = strnlen(file_headers[i].name, sizeof(file_headers[i].name));
            if (name_len > 64) name_len = 64;
            char safe_name[65];
            memcpy(safe_name, file_headers[i].name, name_len);
            safe_name[name_len] = '\0';
            if (strncmp(safe_name, target, tlen) == 0 && safe_name[tlen] == '/') {
                return initrd_make_dir_node(name, target);
            }
        }
        for (initrd_dir_t* p = virtual_dirs; p; p = p->next) {
            if (strncmp(p->name, target, tlen) == 0 && p->name[tlen] == '/') {
                return initrd_make_dir_node(name, target);
            }
        }
    }
    return 0;
}

fs_node_t *initialise_initrd(uint64_t start_addr, uint32_t size) {
    char addr_buf[32];
    hal_console_write("[INITRD] Check at 0x");
    utoa_hex_s(start_addr, addr_buf, sizeof(addr_buf));
    hal_console_write(addr_buf);
    hal_console_write("\n");

    if (start_addr == 0 || size == 0) {
        hal_console_write("[INITRD] No Initrd detected (Address 0 or Size 0).\n");
        return NULL;
    }

    initrd_start = (uint8_t*)start_addr;
    initrd_image_size = size;

    /* Verify header fits */
    if (sizeof(initrd_header_t) > size) {
        hal_console_write("[INITRD] Error: Image too small for header.\n");
        return NULL;
    }

    /* Verify Magic */
    initrd_header_t* header = (initrd_header_t*)initrd_start;
    if (memcmp(header->magic, INITRD_MAGIC, 4) != 0) {
        hal_console_write("[INITRD] Error: Invalid Magic: ");
        char buf[8];
        for (int i=0; i<4; i++) {
             utoa_hex_s(initrd_start[i], buf, sizeof(buf));
             hal_console_write(buf);
             hal_console_write(" ");
        }
        hal_console_write("\n");
        initrd_start = NULL;
        return NULL;
    }

    file_count = header->count;

    /* Verify file headers fit */
    uint64_t headers_size = (uint64_t)file_count * sizeof(initrd_file_header_t);
    if (sizeof(initrd_header_t) + headers_size > size) {
        hal_console_write("[INITRD] Error: File headers exceed image size.\n");
        initrd_start = NULL;
        return NULL;
    }

    file_headers = (initrd_file_header_t*)(initrd_start + sizeof(initrd_header_t));

    hal_console_write("[INITRD] Initialized at 0x");
    char tmp_buf[32];
    utoa_hex_s(start_addr, tmp_buf, sizeof(tmp_buf));
    hal_console_write(tmp_buf);

    hal_console_write(". Found ");
    char count_buf[16];
    itoa_s(file_count, count_buf, sizeof(count_buf), 10);
    hal_console_write(count_buf);
    hal_console_write(" files.\n");

    /* Initialize Virtual Filesystems - REMOVED global init here, done in main */

    initrd_root = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!initrd_root) {
        hal_console_write("[INITRD] Error: Failed to allocate root node.\n");
        return NULL;
    }
    memset(initrd_root, 0, sizeof(fs_node_t));
    initrd_root->ref_count = 1;
    strlcpy(initrd_root->name, "initrd", sizeof(initrd_root->name));
    initrd_root->mask = 0755;
    initrd_root->uid = initrd_root->gid = initrd_root->inode = initrd_root->length = 0;
    initrd_root->flags = FS_DIRECTORY;
    initrd_root->read = 0;
    initrd_root->write = 0;
    initrd_root->open = 0;
    initrd_root->close = 0;
    initrd_root->readdir = &initrd_readdir;
    initrd_root->finddir = &initrd_finddir;
    initrd_root->mkdir = &initrd_mkdir;
    initrd_root->create = &initrd_create;
    initrd_root->ptr = 0;
    initrd_root->impl = 0;

    return initrd_root;
}

/* Legacy helpers wrappers */

void* initrd_read_file(const char* name, uint32_t* size) {
    if (!initrd_start) return NULL;
    for (uint32_t i = 0; i < file_count; i++) {
        size_t name_len = strnlen(file_headers[i].name, sizeof(file_headers[i].name));
        if (strncmp(name, file_headers[i].name, name_len) == 0 && name[name_len] == '\0') {
            /* Security Check */
            if (file_headers[i].offset >= initrd_image_size) return NULL;

            uint32_t file_size = file_headers[i].size;
            if ((uint64_t)file_headers[i].offset + file_size > initrd_image_size) {
                 file_size = (uint32_t)(initrd_image_size - file_headers[i].offset);
            }

            if (size) *size = file_size;
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

        char safe_name[65];
        size_t name_len = strnlen(file_headers[i].name, sizeof(file_headers[i].name));
        memcpy(safe_name, file_headers[i].name, name_len);
        safe_name[name_len] = '\0';
        hal_console_write(safe_name);

        hal_console_write(" (");
        char size_buf[16];
        itoa_s(file_headers[i].size, size_buf, sizeof(size_buf), 10);
        hal_console_write(size_buf);
        hal_console_write(" bytes)\n");
    }
}
uint32_t initrd_get_size(void) { return initrd_image_size; }
