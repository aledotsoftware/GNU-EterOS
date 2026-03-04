#include <fs/procfs.h>
#include <fs/vfs.h>
#include <string.h>
#include <mm.h>
#include <pmm.h>
#include <timer.h>
#include <task.h>

/* Global root node for ProcFS */
static fs_node_t* procfs_root = NULL;

/* ========================================================================= */
/* /proc/version Implementation                                              */
/* ========================================================================= */
static ssize_t proc_version_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
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
static ssize_t proc_uptime_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
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
/* /proc/meminfo Implementation                                              */
/* ========================================================================= */
static ssize_t proc_meminfo_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node;
    char meminfo_str[256];
    char num_buf[32];

    uint64_t total = pmm_get_total_ram();
    uint64_t free = pmm_get_free_ram();
    uint64_t used = pmm_get_used_ram();

    /* Format similar to Linux /proc/meminfo but simplified */
    /* TotalRam: X kB */
    strlcpy(meminfo_str, "MemTotal:       ", sizeof(meminfo_str));
    itoa_s((int64_t)(total / 1024), num_buf, sizeof(num_buf), 10);
    strlcat(meminfo_str, num_buf, sizeof(meminfo_str));
    strlcat(meminfo_str, " kB\n", sizeof(meminfo_str));

    /* MemFree: Y kB */
    strlcat(meminfo_str, "MemFree:        ", sizeof(meminfo_str));
    itoa_s((int64_t)(free / 1024), num_buf, sizeof(num_buf), 10);
    strlcat(meminfo_str, num_buf, sizeof(meminfo_str));
    strlcat(meminfo_str, " kB\n", sizeof(meminfo_str));

    /* MemUsed: Z kB */
    strlcat(meminfo_str, "MemUsed:        ", sizeof(meminfo_str));
    itoa_s((int64_t)(used / 1024), num_buf, sizeof(num_buf), 10);
    strlcat(meminfo_str, num_buf, sizeof(meminfo_str));
    strlcat(meminfo_str, " kB\n", sizeof(meminfo_str));

    size_t len = strlen(meminfo_str);
    if (offset >= len) return 0;
    if (offset + size > len) size = (uint32_t)(len - offset);

    memcpy(buffer, meminfo_str + offset, size);
    return size;
}

/* ========================================================================= */
/* /proc/self Implementation (Dynamic Process Info)                          */
/* ========================================================================= */

// Forward declarations
static fs_node_t *proc_self_finddir(fs_node_t *node, char *name);
static int proc_self_readdir(fs_node_t *node, uint32_t index, struct dirent *entry);
static fs_node_t *proc_self_fd_finddir(fs_node_t *node, char *name);
static int proc_self_fd_readdir(fs_node_t *node, uint32_t index, struct dirent *entry);

/* /proc/self/exe */
static ssize_t proc_self_exe_readlink(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset;
    task_t* current = task_get_current();
    if (!current) return 0;

    int len = strlen(current->executable_path);
    if (len > (int)size) len = size;
    memcpy(buffer, current->executable_path, len);
    return len;
}

/* /proc/self/cmdline */
static ssize_t proc_self_cmdline_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset;
    task_t* current = task_get_current();
    if (!current) return 0;
    // Just return name as cmdline for now
    int len = strlen(current->name) + 1; // include null
    if (len > (int)size) len = size;
    memcpy(buffer, current->name, len);
    return len;
}

/* /proc/self/status */
static ssize_t proc_self_status_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node;
    task_t* current = task_get_current();
    if (!current) return 0;

    char status_str[512];
    char num_buf[32];

    strlcpy(status_str, "Name:\t", sizeof(status_str));
    strlcat(status_str, current->name, sizeof(status_str));
    strlcat(status_str, "\nState:\tR (running)\n", sizeof(status_str));

    strlcat(status_str, "Pid:\t", sizeof(status_str));
    itoa_s((int64_t)current->id, num_buf, sizeof(num_buf), 10);
    strlcat(status_str, num_buf, sizeof(status_str));
    strlcat(status_str, "\n", sizeof(status_str));

    strlcat(status_str, "PPid:\t", sizeof(status_str));
    itoa_s((int64_t)current->parent_id, num_buf, sizeof(num_buf), 10);
    strlcat(status_str, num_buf, sizeof(status_str));
    strlcat(status_str, "\n", sizeof(status_str));

    strlcat(status_str, "Uid:\t", sizeof(status_str));
    itoa_s((int64_t)current->uid, num_buf, sizeof(num_buf), 10);
    strlcat(status_str, num_buf, sizeof(status_str));
    strlcat(status_str, "\t", sizeof(status_str));
    strlcat(status_str, num_buf, sizeof(status_str));
    strlcat(status_str, "\t", sizeof(status_str));
    strlcat(status_str, num_buf, sizeof(status_str));
    strlcat(status_str, "\t", sizeof(status_str));
    strlcat(status_str, num_buf, sizeof(status_str));
    strlcat(status_str, "\n", sizeof(status_str));

    strlcat(status_str, "Gid:\t", sizeof(status_str));
    itoa_s((int64_t)current->gid, num_buf, sizeof(num_buf), 10);
    strlcat(status_str, num_buf, sizeof(status_str));
    strlcat(status_str, "\t", sizeof(status_str));
    strlcat(status_str, num_buf, sizeof(status_str));
    strlcat(status_str, "\t", sizeof(status_str));
    strlcat(status_str, num_buf, sizeof(status_str));
    strlcat(status_str, "\t", sizeof(status_str));
    strlcat(status_str, num_buf, sizeof(status_str));
    strlcat(status_str, "\n", sizeof(status_str));

    size_t len = strlen(status_str);
    if (offset >= len) return 0;
    if (offset + size > len) size = (uint32_t)(len - offset);

    memcpy(buffer, status_str + offset, size);
    return size;
}

/* /proc/self/maps */
static ssize_t proc_self_maps_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node;
    task_t* current = task_get_current();
    if (!current) return 0;

    // Very basic mapping for the whole user range
    char maps_str[256];
    strlcpy(maps_str, "00400000-00800000 r-xp 00000000 00:00 0      /proc/self/exe\n", sizeof(maps_str));
    strlcat(maps_str, "700000000000-7fffffffffff rw-p 00000000 00:00 0      [heap]\n", sizeof(maps_str));
    strlcat(maps_str, "7ffffffde000-7ffffffff000 rw-p 00000000 00:00 0      [stack]\n", sizeof(maps_str));

    size_t len = strlen(maps_str);
    if (offset >= len) return 0;
    if (offset + size > len) size = (uint32_t)(len - offset);

    memcpy(buffer, maps_str + offset, size);
    return size;
}

/* /proc/self/environ */
static ssize_t proc_self_environ_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset;
    // We don't store environ, just return PATH=/bin:/usr/bin
    const char* env = "PATH=/bin:/usr/bin\0USER=root\0";
    size_t len = 30; // approx
    if (size < len) len = size;
    memcpy(buffer, env, len);
    return len;
}

/* /proc/self/fd/ implementation */
static int proc_self_fd_readdir(fs_node_t *node, uint32_t index, struct dirent *entry) {
    (void)node;
    task_t* current = task_get_current();
    if (!current) return -1;

    uint32_t valid_idx = 0;
    for (int i = 0; i < MAX_FD; i++) {
        if (current->fd_table[i].node != NULL) {
            if (valid_idx == index) {
                char num_buf[32];
                itoa_s(i, num_buf, sizeof(num_buf), 10);
                strlcpy(entry->name, num_buf, sizeof(entry->name));
                entry->inode = 1000 + i;
                return 0;
            }
            valid_idx++;
        }
    }
    return 1; /* EOF */
}

static fs_node_t *proc_self_fd_finddir(fs_node_t *node, char *name) {
    (void)node;
    if (!name) return 0;
    task_t* current = task_get_current();
    if (!current) return 0;

    int fd_num = 0;
    for (int i = 0; name[i]; i++) {
        if (name[i] < '0' || name[i] > '9') return 0;
        fd_num = fd_num * 10 + (name[i] - '0');
    }

    if (fd_num < 0 || fd_num >= MAX_FD) return 0;
    if (!current->fd_table[fd_num].node) return 0;

    fs_node_t *fnode = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!fnode) return 0;
    memset(fnode, 0, sizeof(fs_node_t));
    fnode->ref_count = 1;
    fnode->flags = FS_SYMLINK;
    strlcpy(fnode->name, name, sizeof(fnode->name));
    fnode->inode = 1000 + fd_num;
    fnode->read = NULL; // We'd need a readlink here or in VFS. For now, it's a symlink node.
    return fnode;
}


/* /proc/self implementation */
static int proc_self_readdir(fs_node_t *node, uint32_t index, struct dirent *entry) {
    (void)node;
    if (index == 0) { strlcpy(entry->name, "exe", sizeof(entry->name)); entry->inode = 10; return 0; }
    if (index == 1) { strlcpy(entry->name, "cmdline", sizeof(entry->name)); entry->inode = 11; return 0; }
    if (index == 2) { strlcpy(entry->name, "status", sizeof(entry->name)); entry->inode = 12; return 0; }
    if (index == 3) { strlcpy(entry->name, "maps", sizeof(entry->name)); entry->inode = 13; return 0; }
    if (index == 4) { strlcpy(entry->name, "environ", sizeof(entry->name)); entry->inode = 14; return 0; }
    if (index == 5) { strlcpy(entry->name, "fd", sizeof(entry->name)); entry->inode = 15; return 0; }
    return 1;
}

static fs_node_t *proc_self_finddir(fs_node_t *node, char *name) {
    (void)node;
    if (!name) return 0;

    fs_node_t *fnode = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!fnode) return 0;
    memset(fnode, 0, sizeof(fs_node_t));
    fnode->ref_count = 1;

    if (strcmp(name, "exe") == 0) {
        strlcpy(fnode->name, "exe", sizeof(fnode->name));
        fnode->flags = FS_SYMLINK; // Often a symlink in Linux
        fnode->read = proc_self_exe_readlink; // Will act as readlink target
        fnode->inode = 10;
    } else if (strcmp(name, "cmdline") == 0) {
        strlcpy(fnode->name, "cmdline", sizeof(fnode->name));
        fnode->flags = FS_FILE;
        fnode->read = proc_self_cmdline_read;
        fnode->inode = 11;
    } else if (strcmp(name, "status") == 0) {
        strlcpy(fnode->name, "status", sizeof(fnode->name));
        fnode->flags = FS_FILE;
        fnode->read = proc_self_status_read;
        fnode->inode = 12;
    } else if (strcmp(name, "maps") == 0) {
        strlcpy(fnode->name, "maps", sizeof(fnode->name));
        fnode->flags = FS_FILE;
        fnode->read = proc_self_maps_read;
        fnode->inode = 13;
    } else if (strcmp(name, "environ") == 0) {
        strlcpy(fnode->name, "environ", sizeof(fnode->name));
        fnode->flags = FS_FILE;
        fnode->read = proc_self_environ_read;
        fnode->inode = 14;
    } else if (strcmp(name, "fd") == 0) {
        strlcpy(fnode->name, "fd", sizeof(fnode->name));
        fnode->flags = FS_DIRECTORY;
        fnode->readdir = proc_self_fd_readdir;
        fnode->finddir = proc_self_fd_finddir;
        fnode->inode = 15;
    } else {
        kfree(fnode);
        return 0;
    }
    return fnode;
}


/* ========================================================================= */
/* ProcFS Directory Operations                                               */
/* ========================================================================= */
static int procfs_readdir(fs_node_t *node, uint32_t index, struct dirent *entry) {
    (void)node;
    if (index == 0) {
        strlcpy(entry->name, "version", sizeof(entry->name));
        entry->inode = 0;
        return 0;
    }
    if (index == 1) {
        strlcpy(entry->name, "uptime", sizeof(entry->name));
        entry->inode = 1;
        return 0;
    }
    if (index == 2) {
        strlcpy(entry->name, "meminfo", sizeof(entry->name));
        entry->inode = 2;
        return 0;
    }
    if (index == 3) {
        strlcpy(entry->name, "self", sizeof(entry->name));
        entry->inode = 3;
        return 0;
    }
    return 1; /* EOF */
}

static fs_node_t *procfs_finddir(fs_node_t *node, char *name) {
    (void)node;
    if (!name) return 0;

    fs_node_t *fnode = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!fnode) return 0;
    memset(fnode, 0, sizeof(fs_node_t));
    fnode->ref_count = 1;
    fnode->flags = FS_FILE;

    if (strcmp(name, "version") == 0) {
        strlcpy(fnode->name, "version", sizeof(fnode->name));
        fnode->read = proc_version_read;
        fnode->inode = 0;
    } else if (strcmp(name, "uptime") == 0) {
        strlcpy(fnode->name, "uptime", sizeof(fnode->name));
        fnode->read = proc_uptime_read;
        fnode->inode = 1;
    } else if (strcmp(name, "meminfo") == 0) {
        strlcpy(fnode->name, "meminfo", sizeof(fnode->name));
        fnode->read = proc_meminfo_read;
        fnode->inode = 2;
    } else if (strcmp(name, "self") == 0) {
        strlcpy(fnode->name, "self", sizeof(fnode->name));
        fnode->flags = FS_DIRECTORY;
        fnode->readdir = proc_self_readdir;
        fnode->finddir = proc_self_finddir;
        fnode->inode = 3;
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
    procfs_root->ref_count = 1;
    strlcpy(procfs_root->name, "proc", sizeof(procfs_root->name));
    procfs_root->flags = FS_DIRECTORY;
    procfs_root->readdir = procfs_readdir;
    procfs_root->finddir = procfs_finddir;

    return procfs_root;
}
