#include <fs/devfs.h>
#include <fs/vfs.h>
#include <string.h>
#include <mm.h>
#include <hal.h>
#include <keyboard.h>
#include <vga.h>
#include <input/event.h>

/* Global root node for DevFS */
static fs_node_t* devfs_root = NULL;

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
/* /dev/input/event0 (Aggregate) Implementation                              */
/* ========================================================================= */
static uint32_t dev_event_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset;

    if (size < sizeof(input_event_t)) return 0;

    int count = size / sizeof(input_event_t);
    int read = input_read((input_event_t*)buffer, count);

    return read * sizeof(input_event_t);
}

/* ========================================================================= */
/* /dev/input/mouse0 Implementation                                          */
/* ========================================================================= */
static uint32_t dev_mouse_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset;

    if (size < sizeof(input_event_t)) return 0;

    int count = size / sizeof(input_event_t);
    int read = input_read_mouse((input_event_t*)buffer, count);

    return read * sizeof(input_event_t);
}

/* ========================================================================= */
/* /dev/input Directory Implementation                                       */
/* ========================================================================= */
static int devfs_input_readdir(fs_node_t *node, uint32_t index, struct dirent *entry) {
    (void)node;
    if (index == 0) {
        strlcpy(entry->name, "event0", sizeof(entry->name));
        entry->inode = 7; /* Arbitrary unique inode */
        return 0;
    }
    if (index == 1) {
        strlcpy(entry->name, "mouse0", sizeof(entry->name));
        entry->inode = 6; /* Arbitrary unique inode */
        return 0;
    }
    return 1; /* EOF */
}

static fs_node_t *devfs_input_finddir(fs_node_t *node, char *name) {
    (void)node;
    if (!name) return 0;

    fs_node_t *fnode = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!fnode) return 0;
    memset(fnode, 0, sizeof(fs_node_t));
    fnode->ref_count = 1;
    fnode->flags = FS_CHARDEVICE;

    if (strcmp(name, "event0") == 0) {
        strlcpy(fnode->name, "event0", sizeof(fnode->name));
        fnode->read = dev_event_read;
        fnode->inode = 7;
    } else if (strcmp(name, "mouse0") == 0) {
        strlcpy(fnode->name, "mouse0", sizeof(fnode->name));
        fnode->read = dev_mouse_read;
        fnode->inode = 6;
    } else {
        kfree(fnode);
        return 0;
    }
    return fnode;
}


/* ========================================================================= */
/* /dev/random & /dev/urandom Implementation                                 */
/* ========================================================================= */
static uint32_t rand_seed = 123456789;

static uint32_t xorshift32(void) {
    uint32_t x = rand_seed;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    rand_seed = x;
    return x;
}

static uint32_t dev_random_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset;
    /* Mix in TSC for entropy */
    uint32_t lo, hi;
    __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
    rand_seed ^= lo ^ hi;

    for (uint32_t i = 0; i < size; i++) {
        buffer[i] = (uint8_t)(xorshift32() & 0xFF);
    }
    return size;
}

static uint32_t dev_random_write(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset;
    /* Allow writing to mix into pool */
    for (uint32_t i = 0; i < size; i++) {
        rand_seed ^= buffer[i];
        xorshift32();
    }
    return size;
}

/* ========================================================================= */
/* DevFS Directory Operations                                                */
/* ========================================================================= */

static int devfs_readdir(fs_node_t *node, uint32_t index, struct dirent *entry) {
    (void)node;

    if (index == 0) {
        strlcpy(entry->name, "null", sizeof(entry->name));
        entry->inode = 0;
        return 0;
    }
    if (index == 1) {
        strlcpy(entry->name, "zero", sizeof(entry->name));
        entry->inode = 1;
        return 0;
    }
    if (index == 2) {
        strlcpy(entry->name, "tty", sizeof(entry->name));
        entry->inode = 2;
        return 0;
    }
    if (index == 3) {
        strlcpy(entry->name, "random", sizeof(entry->name));
        entry->inode = 3;
        return 0;
    }
    if (index == 4) {
        strlcpy(entry->name, "urandom", sizeof(entry->name));
        entry->inode = 4;
        return 0;
    }
    if (index == 5) {
        strlcpy(entry->name, "input", sizeof(entry->name));
        entry->inode = 5;
        return 0;
    }
    return 1; /* EOF */
}

static fs_node_t *devfs_finddir(fs_node_t *node, char *name) {
    (void)node;
    if (!name) return 0;

    fs_node_t *fnode = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!fnode) return 0;
    memset(fnode, 0, sizeof(fs_node_t));
    fnode->ref_count = 1;
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
    } else if (strcmp(name, "random") == 0) {
        strlcpy(fnode->name, "random", sizeof(fnode->name));
        fnode->read = dev_random_read;
        fnode->write = dev_random_write;
        fnode->inode = 3;
    } else if (strcmp(name, "urandom") == 0) {
        strlcpy(fnode->name, "urandom", sizeof(fnode->name));
        fnode->read = dev_random_read;
        fnode->write = dev_random_write;
        fnode->inode = 4;
    } else if (strcmp(name, "input") == 0) {
        strlcpy(fnode->name, "input", sizeof(fnode->name));
        /* Change from FS_CHARDEVICE to FS_DIRECTORY */
        fnode->flags = FS_DIRECTORY;
        fnode->readdir = devfs_input_readdir;
        fnode->finddir = devfs_input_finddir;
        fnode->read = NULL;
        fnode->inode = 5;
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
    devfs_root->ref_count = 1;
    strlcpy(devfs_root->name, "dev", sizeof(devfs_root->name));
    devfs_root->flags = FS_DIRECTORY;
    devfs_root->readdir = devfs_readdir;
    devfs_root->finddir = devfs_finddir;

    return devfs_root;
}
