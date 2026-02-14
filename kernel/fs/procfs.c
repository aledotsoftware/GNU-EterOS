#include <fs/procfs.h>
#include <fs/vfs.h>
#include <string.h>
#include <mm.h>
#include <timer.h>

/* Global root node for ProcFS */
static fs_node_t* procfs_root = NULL;
static struct dirent procfs_dirent;

/* ========================================================================= */
/* /proc/version Implementation                                              */
/* ========================================================================= */
static uint32_t proc_version_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node;
    const char* version = "eterOS version 0.1.0 (Genesis) (gcc version 12.2.0)\n";
    size_t len = strlen(version);

    if (offset >= len) return 0;
    if (offset + size > len) size = (uint32_t)(len - offset);

    memcpy(buffer, version + offset, size);
    return size;
}

/* ========================================================================= */
/* /proc/uptime Implementation                                               */
/* ========================================================================= */
static uint32_t proc_uptime_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node;

    char uptime_str[64];
    uint32_t uptime = timer_get_uptime_seconds();
    /* Format: "uptime.00 idle.00" (fake idle) */
    char num_buf[32];
    itoa_s((int64_t)uptime, num_buf, sizeof(num_buf), 10);

    strlcpy(uptime_str, num_buf, sizeof(uptime_str));
    strlcat(uptime_str, ".00 0.00\n", sizeof(uptime_str)); // Simple approximation

    size_t len = strlen(uptime_str);
    if (offset >= len) return 0;
    if (offset + size > len) size = (uint32_t)(len - offset);

    memcpy(buffer, uptime_str + offset, size);
    return size;
}

/* ========================================================================= */
/* ProcFS Directory Operations                                               */
/* ========================================================================= */
static struct dirent *procfs_readdir(fs_node_t *node, uint32_t index) {
    (void)node;
    if (index == 0) {
        strlcpy(procfs_dirent.name, "version", sizeof(procfs_dirent.name));
        procfs_dirent.inode = 0;
        return &procfs_dirent;
    }
    if (index == 1) {
        strlcpy(procfs_dirent.name, "uptime", sizeof(procfs_dirent.name));
        procfs_dirent.inode = 1;
        return &procfs_dirent;
    }
    return 0;
}

static fs_node_t *procfs_finddir(fs_node_t *node, char *name) {
    (void)node;
    if (!name) return 0;

    fs_node_t *fnode = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!fnode) return 0;
    memset(fnode, 0, sizeof(fs_node_t));
    fnode->flags = FS_FILE;

    if (strcmp(name, "version") == 0) {
        strlcpy(fnode->name, "version", sizeof(fnode->name));
        fnode->read = proc_version_read;
        fnode->inode = 0;
    } else if (strcmp(name, "uptime") == 0) {
        strlcpy(fnode->name, "uptime", sizeof(fnode->name));
        fnode->read = proc_uptime_read;
        fnode->inode = 1;
    } else {
        kfree(fnode);
        return 0;
    }
    return fnode;
}

fs_node_t* procfs_init(void) {
    procfs_root = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!procfs_root) return NULL;

    memset(procfs_root, 0, sizeof(fs_node_t));
    strlcpy(procfs_root->name, "proc", sizeof(procfs_root->name));
    procfs_root->flags = FS_DIRECTORY;
    procfs_root->readdir = procfs_readdir;
    procfs_root->finddir = procfs_finddir;

    return procfs_root;
}
