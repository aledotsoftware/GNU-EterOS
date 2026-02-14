#include <fs/devfs.h>
#include <fs/vfs.h>
#include <string.h>
#include <mm.h>
#include <hal.h>
#include <keyboard.h>
#include <vga.h>

/* Global root node for DevFS */
static fs_node_t* devfs_root = NULL;

/* Static dirent for readdir */
static struct dirent devfs_dirent;

/* ========================================================================= */
/* /dev/null Implementation                                                  */
/* ========================================================================= */
static uint32_t dev_null_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset; (void)size; (void)buffer;
    return 0; /* EOF */
}

static uint32_t dev_null_write(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset; (void)size; (void)buffer;
    return size; /* Discard data, return success */
}

/* ========================================================================= */
/* /dev/zero Implementation                                                  */
/* ========================================================================= */
static uint32_t dev_zero_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset;
    memset(buffer, 0, size);
    return size;
}

static uint32_t dev_zero_write(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset; (void)size; (void)buffer;
    return size; /* Discard */
}

/* ========================================================================= */
/* /dev/tty Implementation                                                   */
/* ========================================================================= */
static uint32_t dev_tty_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset;
    for (uint32_t i = 0; i < size; i++) {
        buffer[i] = (uint8_t)keyboard_getchar();
        /* Line buffered simulation (optional) */
        if (buffer[i] == '\n') return i+1;
    }
    return size;
}

static uint32_t dev_tty_write(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset;
    for (uint32_t i = 0; i < size; i++) {
        terminal_putchar((char)buffer[i]);
    }
    return size;
}

/* ========================================================================= */
/* DevFS Directory Operations                                                */
/* ========================================================================= */

static struct dirent *devfs_readdir(fs_node_t *node, uint32_t index) {
    (void)node;

    if (index == 0) {
        strlcpy(devfs_dirent.name, "null", sizeof(devfs_dirent.name));
        devfs_dirent.inode = 0;
        return &devfs_dirent;
    }
    if (index == 1) {
        strlcpy(devfs_dirent.name, "zero", sizeof(devfs_dirent.name));
        devfs_dirent.inode = 1;
        return &devfs_dirent;
    }
    if (index == 2) {
        strlcpy(devfs_dirent.name, "tty", sizeof(devfs_dirent.name));
        devfs_dirent.inode = 2;
        return &devfs_dirent;
    }
    return 0;
}

static fs_node_t *devfs_finddir(fs_node_t *node, char *name) {
    (void)node;
    if (!name) return 0;

    fs_node_t *fnode = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!fnode) return 0;
    memset(fnode, 0, sizeof(fs_node_t));
    fnode->flags = FS_CHARDEVICE;

    if (strcmp(name, "null") == 0) {
        strlcpy(fnode->name, "null", sizeof(fnode->name));
        fnode->read = dev_null_read;
        fnode->write = dev_null_write;
        fnode->inode = 0;
    } else if (strcmp(name, "zero") == 0) {
        strlcpy(fnode->name, "zero", sizeof(fnode->name));
        fnode->read = dev_zero_read;
        fnode->write = dev_zero_write;
        fnode->inode = 1;
    } else if (strcmp(name, "tty") == 0) {
        strlcpy(fnode->name, "tty", sizeof(fnode->name));
        fnode->read = dev_tty_read;
        fnode->write = dev_tty_write;
        fnode->inode = 2;
    } else {
        kfree(fnode);
        return 0;
    }
    return fnode;
}

fs_node_t* devfs_init(void) {
    devfs_root = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!devfs_root) return NULL;

    memset(devfs_root, 0, sizeof(fs_node_t));
    strlcpy(devfs_root->name, "dev", sizeof(devfs_root->name));
    devfs_root->flags = FS_DIRECTORY;
    devfs_root->readdir = devfs_readdir;
    devfs_root->finddir = devfs_finddir;

    return devfs_root;
}
