#include <fs/devfs.h>
#include <fs/vfs.h>
#include <string.h>
#include <mm.h>
#include <hal.h>
#include <keyboard.h>
#include <vga.h>
#include <input/event.h>
#include <lock.h>
#include <ioctl.h>
#include <crypto/sha256.h>
#include <framebuffer.h>
#include <serial.h>

/* Global root node for DevFS */
static fs_node_t* devfs_root = NULL;
static uint32_t dev_mouse_read_debug = 0;

static void devfs_debug_write_i32(int32_t value) {
    char buf[16];
    int i = 0;
    uint32_t magnitude;

    if (value == 0) {
        serial_write_string("0");
        return;
    }

    if (value < 0) {
        serial_write_string("-");
        magnitude = (uint32_t)(-value);
    } else {
        magnitude = (uint32_t)value;
    }

    while (magnitude > 0 && i < (int)sizeof(buf)) {
        buf[i++] = (char)('0' + (magnitude % 10));
        magnitude /= 10;
    }

    while (i-- > 0) {
        char out[2] = { buf[i], '\0' };
        serial_write_string(out);
    }
}

/* ========================================================================= */
/* /dev/null Implementation                                                  */
/* ========================================================================= */
static ssize_t dev_null_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
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
static ssize_t dev_zero_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
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
static ssize_t dev_tty_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
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
static ssize_t dev_event_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset;

    if (size < sizeof(input_event_t)) return 0;

    int count = size / sizeof(input_event_t);
    int read = input_read((input_event_t*)buffer, count);

    return read * sizeof(input_event_t);
}

static int dev_event_ioctl(fs_node_t *node, int request, void *arg) {
    (void)node;
    if (!arg) return -1;
    if (request == FIONREAD) {
        *(int*)arg = input_pending() * (int)sizeof(input_event_t);
        return 0;
    }
    return -1;
}

/* ========================================================================= */
/* /dev/input/mouse0 Implementation                                          */
/* ========================================================================= */
static ssize_t dev_mouse_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset;

    if (size < sizeof(input_event_t)) return 0;

    int count = size / sizeof(input_event_t);
    int read = input_read_mouse((input_event_t*)buffer, count);

    if (read > 0 && dev_mouse_read_debug < 12) {
        input_event_t *ev = (input_event_t*)buffer;
        serial_write_string("[DEVFS] mouse0 read type=");
        devfs_debug_write_i32(ev[0].type);
        serial_write_string(" code=");
        devfs_debug_write_i32(ev[0].code);
        serial_write_string(" value=");
        devfs_debug_write_i32(ev[0].value);
        serial_write_string("\n");
        dev_mouse_read_debug++;
    }

    return read * sizeof(input_event_t);
}

static int dev_mouse_ioctl(fs_node_t *node, int request, void *arg) {
    (void)node;
    if (!arg) return -1;
    if (request == FIONREAD) {
        *(int*)arg = input_mouse_pending() * (int)sizeof(input_event_t);
        return 0;
    }
    return -1;
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
        fnode->ioctl = dev_event_ioctl;
        fnode->inode = 7;
    } else if (strcmp(name, "mouse0") == 0) {
        strlcpy(fnode->name, "mouse0", sizeof(fnode->name));
        fnode->read = dev_mouse_read;
        fnode->ioctl = dev_mouse_ioctl;
        fnode->inode = 6;
    } else {
        kfree(fnode);
        return 0;
    }
    return fnode;
}


/* ========================================================================= */
/* /dev/random & /dev/urandom Implementation (CSPRNG)                        */
/* ========================================================================= */
static uint8_t entropy_pool[32] = {
    0x12, 0x34, 0x56, 0x78, 0x9a, 0xbc, 0xde, 0xf0,
    0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88,
    0x99, 0xaa, 0xbb, 0xcc, 0xdd, 0xee, 0xff, 0x00,
    0xab, 0xcd, 0xef, 0x01, 0x23, 0x45, 0x67, 0x89
};
static spinlock_t rng_lock = 0;

void get_random_bytes(uint8_t *buf, size_t len) {
    spin_lock(&rng_lock);
    while (len > 0) {
        /* Mix in TSC for additional entropy */
        uint32_t lo, hi;
        __asm__ volatile ("rdtsc" : "=a"(lo), "=d"(hi));
        entropy_pool[0] ^= (lo & 0xFF);
        entropy_pool[1] ^= ((lo >> 8) & 0xFF);
        entropy_pool[2] ^= ((lo >> 16) & 0xFF);
        entropy_pool[3] ^= ((lo >> 24) & 0xFF);
        entropy_pool[4] ^= (hi & 0xFF);
        entropy_pool[5] ^= ((hi >> 8) & 0xFF);

        /* Hash the pool to generate output and update the state */
        uint8_t hash[32];
        sha256(entropy_pool, sizeof(entropy_pool), hash);

        /* Copy up to 32 bytes to the output buffer */
        size_t copy_len = (len < sizeof(hash)) ? len : sizeof(hash);
        memcpy(buf, hash, copy_len);

        /* Update the entropy pool with the hash to prevent backtracking */
        sha256(hash, sizeof(hash), entropy_pool);

        buf += copy_len;
        len -= copy_len;
    }
    spin_unlock(&rng_lock);
}

static ssize_t dev_random_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset;
    get_random_bytes(buffer, size);
    return size;
}

static uint32_t dev_random_write(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset;

    /* Mix user data into the entropy pool */
    spin_lock(&rng_lock);

    SHA256_CTX ctx;
    sha256_init(&ctx);
    sha256_update(&ctx, entropy_pool, sizeof(entropy_pool));
    sha256_update(&ctx, buffer, size);
    sha256_final(&ctx, entropy_pool);

    spin_unlock(&rng_lock);
    return size;
}

/* ========================================================================= */
/* /dev/fb0 Implementation                                                   */
/* ========================================================================= */
static int dev_fb0_ioctl(fs_node_t *node, int request, void *arg) {
    (void)node;
    if (request == 0x4600) { /* FBIOGET_VSCREENINFO */
        uint32_t* info = (uint32_t*)arg;
        if (!info) return -1;
        info[0] = framebuffer_get_width();
        info[1] = framebuffer_get_height();
        info[2] = framebuffer_get_bpp();
        info[3] = framebuffer_get_pitch();
        return 0;
    }
    return -1;
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
    if (index == 6) {
        strlcpy(entry->name, "fb0", sizeof(entry->name));
        entry->inode = 6;
        return 0;
    }
    if (index == 7) {
        strlcpy(entry->name, "shm", sizeof(entry->name));
        entry->inode = 7;
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
    } else if (strcmp(name, "fb0") == 0) {
        strlcpy(fnode->name, "fb0", sizeof(fnode->name));
        fnode->flags = FS_CHARDEVICE;
        fnode->ioctl = dev_fb0_ioctl;
        fnode->inode = 6;
    } else if (strcmp(name, "shm") == 0) {
        strlcpy(fnode->name, "shm", sizeof(fnode->name));
        fnode->flags = FS_DIRECTORY;
        fnode->inode = 7;
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
