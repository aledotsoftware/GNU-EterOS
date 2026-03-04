#ifndef FS_VFS_H
#define FS_VFS_H

#include <types.h>
#include <lock.h>

#define FS_FILE        0x01
#define FS_DIRECTORY   0x02
#define FS_CHARDEVICE  0x03
#define FS_BLOCKDEVICE 0x04
#define FS_PIPE        0x05
#define FS_SYMLINK     0x06
#define FS_SOCKET      0x07
#define FS_MOUNTPOINT  0x08

struct fs_node;

struct dirent {
    char name[128];
    uint32_t inode;
};

struct linux_dirent64 {
    uint64_t d_ino;
    int64_t  d_off;
    uint16_t d_reclen;
    uint8_t  d_type;
    char     d_name[];
};

#define DT_UNKNOWN 0
#define DT_FIFO    1
#define DT_CHR     2
#define DT_DIR     4
#define DT_BLK     6
#define DT_REG     8
#define DT_LNK     10
#define DT_SOCK    12
#define DT_WHT     14

typedef ssize_t (*read_type_t)(struct fs_node*, uint32_t, uint32_t, uint8_t*);
typedef uint32_t (*write_type_t)(struct fs_node*, uint32_t, uint32_t, uint8_t*);
typedef void (*open_type_t)(struct fs_node*);
typedef void (*close_type_t)(struct fs_node*);
typedef int (*readdir_type_t)(struct fs_node*, uint32_t, struct dirent*);
typedef struct fs_node * (*finddir_type_t)(struct fs_node*, char *name);
typedef int (*create_type_t)(struct fs_node*, char*, uint16_t);
typedef int (*mkdir_type_t)(struct fs_node*, char*, uint16_t);
typedef int (*unlink_type_t)(struct fs_node*, char*);
typedef int (*ioctl_type_t)(struct fs_node*, int, void*);

typedef struct fs_node {
    char name[128];
    uint32_t mask;        /* The permissions mask */
    uint32_t uid;         /* The owning user */
    uint32_t gid;         /* The owning group */
    uint32_t flags;       /* Includes the node type */
    uint32_t inode;       /* This is device-specific - provides a way for a filesystem to identify files */
    uint32_t length;      /* Size of the file, in bytes */
    uint32_t impl;        /* An implementation-defined number */
    time_t   atime;       /* Access time */
    time_t   mtime;       /* Modification time */
    time_t   ctime;       /* Creation time */
    read_type_t read;
    write_type_t write;
    open_type_t open;
    close_type_t close;
    ioctl_type_t ioctl;
    readdir_type_t readdir;
    finddir_type_t finddir;
    create_type_t create;
    mkdir_type_t mkdir;
    unlink_type_t unlink;
    struct fs_node *ptr; /* Used by mountpoints and symlinks */
    uint32_t ref_count;   /* Reference counting for shared nodes */
    spinlock_t lock;      /* SMP lock for this node */
} fs_node_t;

/* Standard read/write/open/close functions. Note that these are all suffixed with
 * _fs to distinguish them from the read/write/open/close which deal with file descriptors
 * , not file nodes.
 */
ssize_t read_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
uint32_t write_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
void open_fs(fs_node_t *node, uint8_t read, uint8_t write);
void close_fs(fs_node_t *node);
int readdir_fs(fs_node_t *node, uint32_t index, struct dirent *entry);
fs_node_t *finddir_fs(fs_node_t *node, char *name);
int create_fs(fs_node_t *parent, char *name, uint16_t permission);
int mkdir_fs(fs_node_t *parent, char *name, uint16_t permission);
int unlink_fs(fs_node_t *parent, char *name);
int ioctl_fs(fs_node_t *node, int request, void *arg);

int vfs_mount(const char *path, fs_node_t *fs);
int vfs_mkdir(const char *path, uint16_t permission);

/**
 * Resolves a path to a filesystem node.
 *
 * @param root The root node to start the search from (usually fs_root).
 * @param path The path string to look up.
 * @param follow_symlink If 0, does not resolve the final symlink component. If 1, resolves it.
 * @return A pointer to the found fs_node_t, or NULL if not found.
 *
 * @note The returned node is allocated with kmalloc() and MUST be freed
 *       by the caller using kfree() when it is no longer needed to prevent memory leaks.
 */
fs_node_t *vfs_lookup_ext(fs_node_t *root, const char *path, int follow_symlink);

/**
 * Backwards compatible vfs_lookup that always follows symlinks.
 */
fs_node_t *vfs_lookup(fs_node_t *root, const char *path);

/**
 * Normalizes a path, resolving relative components like '..' and '.',
 * and handles relative paths based on a provided directory string.
 *
 * @param out_path The buffer to store the normalized absolute path.
 * @param size The size of out_path buffer.
 * @param path The path to normalize.
 * @param base_dir The base directory to use if path is relative. If NULL, task CWD is used.
 * @return 0 on success, -1 on error (e.g. path too long or going above root).
 */
int vfs_normalize_path(char* out_path, int size, const char* path, const char* base_dir);

extern fs_node_t *fs_root;

#endif
