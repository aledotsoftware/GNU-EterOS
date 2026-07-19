# eterOS Virtual File System (VFS) API

The VFS layer abstracts different file systems (InitRD, FAT32, JFS, DevFS, ProcFS) into a single, unified hierarchical tree.

## Core Structures

```c
typedef struct fs_node {
    char name[128];
    uint32_t mask;      // Permissions (rwxrwxrwx)
    uint32_t flags;     // Node type (FS_FILE, FS_DIRECTORY, etc.)
    uint32_t inode;     // Filesystem-specific inode number
    uint32_t length;    // Size of the file in bytes
    uint32_t impl;      // Filesystem-specific implementation data

    // Function pointers for filesystem-specific operations
    read_type_t read;
    write_type_t write;
    open_type_t open;
    close_type_t close;
    readdir_type_t readdir;
    finddir_type_t finddir;
    // ...
} fs_node_t;
```

## Global Operations

```c
fs_node_t *vfs_lookup(fs_node_t *root, const char *path);
```
Traverses the VFS tree starting from `root` to find the node corresponding to `path`. Follows symlinks by default.

```c
int vfs_normalize_path(char* out_path, int size, const char* path, const char* base_dir);
```
Resolves a given `path` (absolute or relative) against `base_dir` (or the current process's CWD if NULL) into an absolute, normalized path without `.` or `..` components.

## Node Operations

```c
ssize_t read_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
```
Reads `size` bytes from `node` at `offset` into `buffer`.

```c
uint32_t write_fs(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer);
```
Writes `size` bytes to `node` at `offset` from `buffer`.

```c
int readdir_fs(fs_node_t *node, uint32_t index, struct dirent *entry);
```
Reads the directory entry at the specified `index` within the directory `node`.

```c
fs_node_t *finddir_fs(fs_node_t *node, char *name);
```
Searches for a child node named `name` within the directory `node`.
