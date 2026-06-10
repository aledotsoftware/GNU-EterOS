#include <fs/procfs.h>
#include <errno.h>
#include <fs/vfs.h>
#include <string.h>
#include <mm.h>
#include <pmm.h>
#include <timer.h>
#include <task.h>

/* Global root node for ProcFS */
static fs_node_t* procfs_root = NULL;

static task_t* proc_node_task(fs_node_t *node) {
    if (!node || node->impl == 0) {
        return task_get_current();
    }
    return task_get_by_id(node->impl);
}

static int proc_node_fd(fs_node_t *node) {
    if (!node || node->inode < 1000) return -1;
    return (int)(node->inode - 1000);
}

static char proc_task_state_char(task_t* t) {
    if (!t) return '?';
    switch (t->state) {
        case TASK_RUNNING: return 'R';
        case TASK_READY: return 'R';
        case TASK_SLEEPING: return 'S';
        case TASK_BLOCKED: return 'D';
        case TASK_STOPPED: return 'T';
        case TASK_DEAD: return 'Z';
        default: return '?';
    }
}

static const char* proc_task_state_word(task_t* t) {
    if (!t) return "unknown";
    switch (t->state) {
        case TASK_RUNNING: return "running";
        case TASK_READY: return "running";
        case TASK_SLEEPING: return "sleeping";
        case TASK_BLOCKED: return "blocked";
        case TASK_STOPPED: return "stopped";
        case TASK_DEAD: return "zombie";
        default: return "unknown";
    }
}

static void proc_append_int(char* out, size_t out_sz, int64_t v) {
    char b[32];
    itoa_s(v, b, sizeof(b), 10);
    strlcat(out, b, out_sz);
}

/* ========================================================================= */
/* /proc/version Implementation                                              */
/* ========================================================================= */
static ssize_t proc_version_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node;
    const char* version = "eterOS version 0.2.0 (Genesis SMP) (gcc version 12.2.0)\n";
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
    (void)offset;
    task_t* current = proc_node_task(node);
    if (!current || !buffer) return 0;

    int len = strlen(current->executable_path);
    if (len > (int)size) len = size;
    memcpy(buffer, current->executable_path, len);
    return len;
}

/* /proc/self/cmdline */
static ssize_t proc_self_cmdline_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)offset;
    task_t* current = proc_node_task(node);
    if (!current || !buffer) return 0;
    const char* cmd = current->executable_path[0] ? current->executable_path : current->name;
    int len = strlen(cmd) + 1; /* include trailing NUL */
    if (len > (int)size) len = size;
    memcpy(buffer, cmd, len);
    return len;
}

/* /proc/self/status */
static ssize_t proc_self_status_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    task_t* current = proc_node_task(node);
    if (!current || !buffer) return 0;

    char status_str[512];
    char state_buf[8];
    state_buf[0] = proc_task_state_char(current);
    state_buf[1] = '\0';

    strlcpy(status_str, "Name:\t", sizeof(status_str));
    strlcat(status_str, current->name, sizeof(status_str));
    strlcat(status_str, "\nState:\t", sizeof(status_str));
    strlcat(status_str, state_buf, sizeof(status_str));
    strlcat(status_str, " (", sizeof(status_str));
    strlcat(status_str, proc_task_state_word(current), sizeof(status_str));
    strlcat(status_str, ")\n", sizeof(status_str));

    strlcat(status_str, "Pid:\t", sizeof(status_str));
    proc_append_int(status_str, sizeof(status_str), (int64_t)current->id);
    strlcat(status_str, "\n", sizeof(status_str));

    strlcat(status_str, "PPid:\t", sizeof(status_str));
    proc_append_int(status_str, sizeof(status_str), (int64_t)current->parent_id);
    strlcat(status_str, "\n", sizeof(status_str));

    strlcat(status_str, "Tgid:\t", sizeof(status_str));
    proc_append_int(status_str, sizeof(status_str), (int64_t)current->tgid);
    strlcat(status_str, "\n", sizeof(status_str));

    strlcat(status_str, "Pgid:\t", sizeof(status_str));
    proc_append_int(status_str, sizeof(status_str), (int64_t)current->pgid);
    strlcat(status_str, "\n", sizeof(status_str));

    strlcat(status_str, "Sid:\t", sizeof(status_str));
    proc_append_int(status_str, sizeof(status_str), (int64_t)current->sid);
    strlcat(status_str, "\n", sizeof(status_str));

    strlcat(status_str, "Uid:\t", sizeof(status_str));
    proc_append_int(status_str, sizeof(status_str), (int64_t)current->uid);
    strlcat(status_str, "\t", sizeof(status_str));
    proc_append_int(status_str, sizeof(status_str), (int64_t)current->euid);
    strlcat(status_str, "\t", sizeof(status_str));
    proc_append_int(status_str, sizeof(status_str), (int64_t)current->uid);
    strlcat(status_str, "\t", sizeof(status_str));
    proc_append_int(status_str, sizeof(status_str), (int64_t)current->euid);
    strlcat(status_str, "\n", sizeof(status_str));

    strlcat(status_str, "Gid:\t", sizeof(status_str));
    proc_append_int(status_str, sizeof(status_str), (int64_t)current->gid);
    strlcat(status_str, "\t", sizeof(status_str));
    proc_append_int(status_str, sizeof(status_str), (int64_t)current->egid);
    strlcat(status_str, "\t", sizeof(status_str));
    proc_append_int(status_str, sizeof(status_str), (int64_t)current->gid);
    strlcat(status_str, "\t", sizeof(status_str));
    proc_append_int(status_str, sizeof(status_str), (int64_t)current->egid);
    strlcat(status_str, "\n", sizeof(status_str));

    strlcat(status_str, "Threads:\t1\n", sizeof(status_str));
    strlcat(status_str, "VmSize:\t", sizeof(status_str));
    proc_append_int(status_str, sizeof(status_str), (int64_t)(current->brk / 1024));
    strlcat(status_str, " kB\n", sizeof(status_str));
    strlcat(status_str, "VmRSS:\t0 kB\n", sizeof(status_str));
    strlcat(status_str, "FDSize:\t", sizeof(status_str));
    proc_append_int(status_str, sizeof(status_str), MAX_FD);
    strlcat(status_str, "\n", sizeof(status_str));

    size_t len = strlen(status_str);
    if (offset >= len) return 0;
    if (offset + size > len) size = (uint32_t)(len - offset);

    memcpy(buffer, status_str + offset, size);
    return size;
}

/* /proc/self/stat */
static ssize_t proc_self_stat_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    task_t* current = proc_node_task(node);
    if (!current || !buffer) return 0;

    char stat_str[512];
    char st[2];
    st[0] = proc_task_state_char(current);
    st[1] = '\0';

    strlcpy(stat_str, "", sizeof(stat_str));
    proc_append_int(stat_str, sizeof(stat_str), (int64_t)current->id);
    strlcat(stat_str, " (", sizeof(stat_str));
    strlcat(stat_str, current->name, sizeof(stat_str));
    strlcat(stat_str, ") ", sizeof(stat_str));
    strlcat(stat_str, st, sizeof(stat_str));
    strlcat(stat_str, " ", sizeof(stat_str));
    proc_append_int(stat_str, sizeof(stat_str), (int64_t)current->parent_id); /* ppid */
    strlcat(stat_str, " ", sizeof(stat_str));
    proc_append_int(stat_str, sizeof(stat_str), (int64_t)current->pgid); /* pgrp */
    strlcat(stat_str, " ", sizeof(stat_str));
    proc_append_int(stat_str, sizeof(stat_str), (int64_t)current->sid); /* session */
    strlcat(stat_str, " 0 ", sizeof(stat_str)); /* tty_nr */
    proc_append_int(stat_str, sizeof(stat_str), (int64_t)current->pgid); /* tpgid */
    strlcat(stat_str, " 0 0 0 0 0 ", sizeof(stat_str)); /* flags/minflt/cminflt/majflt/cmajflt */
    strlcat(stat_str, "0 0 0 0 ", sizeof(stat_str)); /* utime/stime/cutime/cstime */
    strlcat(stat_str, "20 0 1 0 ", sizeof(stat_str)); /* priority/nice/threads/itrealvalue */
    strlcat(stat_str, "0 ", sizeof(stat_str)); /* starttime */
    proc_append_int(stat_str, sizeof(stat_str), (int64_t)current->brk); /* vsize */
    strlcat(stat_str, " 0\n", sizeof(stat_str)); /* rss */

    size_t len = strlen(stat_str);
    if (offset >= len) return 0;
    if (offset + size > len) size = (uint32_t)(len - offset);
    memcpy(buffer, stat_str + offset, size);
    return size;
}

/* /proc/self/maps */
static ssize_t proc_self_maps_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    task_t* current = proc_node_task(node);
    if (!current || !buffer) return 0;

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
    if (!buffer) return 0;
    // We don't store environ, just return PATH=/bin:/usr/bin
    const char* env = "PATH=/bin:/usr/bin\0USER=root\0";
    size_t len = 30; // approx
    if (size < len) len = size;
    memcpy(buffer, env, len);
    return len;
}

static ssize_t proc_fd_link_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)offset;
    task_t* target = proc_node_task(node);
    int fd_num = proc_node_fd(node);
    if (!target || !buffer || fd_num < 0 || fd_num >= MAX_FD) return 0;
    if (!target->fd_table[fd_num].node) return 0;

    const char* p = target->fd_table[fd_num].path;
    if (!p || p[0] == '\0') p = "<anon>";
    int len = strlen(p);
    if (len > (int)size) len = (int)size;
    memcpy(buffer, p, len);
    return len;
}

/* /proc/self/fd/ implementation */
static int proc_self_fd_readdir(fs_node_t *node, uint32_t index, struct dirent *entry) {
    task_t* current = proc_node_task(node);
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
    if (!name) return 0;
    task_t* current = proc_node_task(node);
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
    fnode->mask = 0777;
    fnode->inode = 1000 + fd_num;
    fnode->impl = current->id;
    fnode->read = proc_fd_link_read;
    return fnode;
}


/* /proc/self implementation */
static int proc_self_readdir(fs_node_t *node, uint32_t index, struct dirent *entry) {
    (void)node;
    if (index == 0) { strlcpy(entry->name, "exe", sizeof(entry->name)); entry->inode = 10; return 0; }
    if (index == 1) { strlcpy(entry->name, "cmdline", sizeof(entry->name)); entry->inode = 11; return 0; }
    if (index == 2) { strlcpy(entry->name, "status", sizeof(entry->name)); entry->inode = 12; return 0; }
    if (index == 3) { strlcpy(entry->name, "stat", sizeof(entry->name)); entry->inode = 13; return 0; }
    if (index == 4) { strlcpy(entry->name, "maps", sizeof(entry->name)); entry->inode = 14; return 0; }
    if (index == 5) { strlcpy(entry->name, "environ", sizeof(entry->name)); entry->inode = 15; return 0; }
    if (index == 6) { strlcpy(entry->name, "fd", sizeof(entry->name)); entry->inode = 16; return 0; }
    return 1;
}

static fs_node_t *proc_self_finddir(fs_node_t *node, char *name) {
    if (!name) return 0;

    fs_node_t *fnode = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!fnode) return 0;
    memset(fnode, 0, sizeof(fs_node_t));
    fnode->ref_count = 1;
    fnode->impl = node ? node->impl : 0;

    if (strcmp(name, "exe") == 0) {
        strlcpy(fnode->name, "exe", sizeof(fnode->name));
        fnode->flags = FS_SYMLINK; // Often a symlink in Linux
        fnode->read = proc_self_exe_readlink; // Will act as readlink target
        fnode->mask = 0777;
        fnode->inode = 10;
    } else if (strcmp(name, "cmdline") == 0) {
        strlcpy(fnode->name, "cmdline", sizeof(fnode->name));
        fnode->flags = FS_FILE;
        fnode->read = proc_self_cmdline_read;
        fnode->mask = 0444;
        fnode->inode = 11;
    } else if (strcmp(name, "status") == 0) {
        strlcpy(fnode->name, "status", sizeof(fnode->name));
        fnode->flags = FS_FILE;
        fnode->read = proc_self_status_read;
        fnode->mask = 0444;
        fnode->inode = 12;
    } else if (strcmp(name, "stat") == 0) {
        strlcpy(fnode->name, "stat", sizeof(fnode->name));
        fnode->flags = FS_FILE;
        fnode->read = proc_self_stat_read;
        fnode->mask = 0444;
        fnode->inode = 13;
    } else if (strcmp(name, "maps") == 0) {
        strlcpy(fnode->name, "maps", sizeof(fnode->name));
        fnode->flags = FS_FILE;
        fnode->read = proc_self_maps_read;
        fnode->mask = 0444;
        fnode->inode = 14;
    } else if (strcmp(name, "environ") == 0) {
        strlcpy(fnode->name, "environ", sizeof(fnode->name));
        fnode->flags = FS_FILE;
        fnode->read = proc_self_environ_read;
        fnode->mask = 0444;
        fnode->inode = 15;
    } else if (strcmp(name, "fd") == 0) {
        strlcpy(fnode->name, "fd", sizeof(fnode->name));
        fnode->flags = FS_DIRECTORY;
        fnode->readdir = proc_self_fd_readdir;
        fnode->finddir = proc_self_fd_finddir;
        fnode->mask = 0555;
        fnode->inode = 16;
    } else {
        kfree(fnode);
        return 0;
    }
    return fnode;
}


static ssize_t proc_self_symlink_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset;
    task_t* current = task_get_current();
    if (!current) return 0;

    char pid_str[32];
    itoa_s((int64_t)current->id, pid_str, sizeof(pid_str), 10);

    char link_target[64];
    strlcpy(link_target, "/proc/", sizeof(link_target));
    strlcat(link_target, pid_str, sizeof(link_target));

    int len = strlen(link_target);
    if (len > (int)size) len = size;
    memcpy(buffer, link_target, len);
    return len;
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

    index -= 4; /* Shift index for task entries */

    uint32_t seen = 0;
    int max = task_get_max();
    for (int i = 0; i < max; i++) {
        task_t* t = task_get_at(i);
        if (!t) continue;
        if (t->state == TASK_DEAD) continue;
        if (t->id == 0 && t->name[0] == '\0') continue;
        if (seen == index) {
            char pid_str[32];
            itoa_s((int64_t)t->id, pid_str, sizeof(pid_str), 10);
            strlcpy(entry->name, pid_str, sizeof(entry->name));
            entry->inode = 100 + t->id;
            return 0;
        }
        seen++;
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
        fnode->mask = 0444;
        fnode->inode = 0;
    } else if (strcmp(name, "uptime") == 0) {
        strlcpy(fnode->name, "uptime", sizeof(fnode->name));
        fnode->read = proc_uptime_read;
        fnode->mask = 0444;
        fnode->inode = 1;
    } else if (strcmp(name, "meminfo") == 0) {
        strlcpy(fnode->name, "meminfo", sizeof(fnode->name));
        fnode->read = proc_meminfo_read;
        fnode->mask = 0444;
        fnode->inode = 2;
    } else if (strcmp(name, "self") == 0) {
        strlcpy(fnode->name, "self", sizeof(fnode->name));
        fnode->flags = FS_SYMLINK;
        fnode->read = proc_self_symlink_read;
        fnode->mask = 0777;
        fnode->inode = 3;
    } else {
        // Attempt to parse name as PID
        int pid = 0;
        int is_pid = 1;
        for (int i = 0; name[i]; i++) {
            if (name[i] >= '0' && name[i] <= '9') {
                pid = pid * 10 + (name[i] - '0');
            } else {
                is_pid = 0;
                break;
            }
        }

        if (is_pid) {
            task_t* target = task_get_by_id((uint32_t)pid);
            if (!target || target->state == TASK_DEAD) {
                kfree(fnode);
                return 0;
            }
            strlcpy(fnode->name, name, sizeof(fnode->name));
            fnode->flags = FS_DIRECTORY;
            fnode->readdir = proc_self_readdir;
            fnode->finddir = proc_self_finddir;
            fnode->mask = 0555;
            fnode->inode = 100 + pid;
            fnode->impl = (uint32_t)pid;
            return fnode;
        }

        kfree(fnode);
        return 0;
    }
    return fnode;
}

static int procfs_create(fs_node_t *parent, char *name, uint16_t permission) {
    (void)parent; (void)name; (void)permission;
    return -EROFS;
}

static int procfs_mkdir(fs_node_t *parent, char *name, uint16_t permission) {
    (void)parent; (void)name; (void)permission;
    return -EROFS;
}

fs_node_t* procfs_init(void) {
    procfs_root = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!procfs_root) return NULL;

    memset(procfs_root, 0, sizeof(fs_node_t));
    procfs_root->ref_count = 1;
    strlcpy(procfs_root->name, "proc", sizeof(procfs_root->name));
    procfs_root->flags = FS_DIRECTORY;
    procfs_root->mask = 0555;
    procfs_root->readdir = procfs_readdir;
    procfs_root->finddir = procfs_finddir;
    procfs_root->create = procfs_create;
    procfs_root->mkdir = procfs_mkdir;

    return procfs_root;
}
