#include <fs/vfs.h>
#include <keyboard.h>
#include <vga.h>
#include <serial.h>
#include <mm.h>
#include <string.h>
#include <types.h>

/* TTY Node Functions */

static uint32_t tty_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset;

    if (size == 0) return 0;

    /* Read from keyboard buffer */
    /* This blocks until input is available */
    uint32_t i = 0;
    while (i < size) {
        /* TODO: Handle non-blocking if O_NONBLOCK is set */
        char c = keyboard_getchar();

        /* Echo to screen and serial */
        terminal_putchar(c);
        serial_putchar(c);

        /* Handle Backspace */
        if (c == '\b') {
            if (i > 0) {
                i--;
                /* Erase from screen: Backspace + Space + Backspace */
                terminal_putchar(' ');
                terminal_putchar('\b');
                /* Serial backspace */
                serial_putchar(' ');
                serial_putchar('\b');
            }
            continue;
        }

        buffer[i++] = c;
        if (c == '\n' || c == '\r') {
            /* Normalize newline */
            if (c == '\r') buffer[i-1] = '\n';
            return i;
        }
    }
    return i;
}

static uint32_t tty_write(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset;

    for (uint32_t i = 0; i < size; i++) {
        terminal_putchar(buffer[i]);
        serial_putchar(buffer[i]);
    }
    return size;
}

static int tty_ioctl(fs_node_t *node, int request, void *arg) {
    (void)node; (void)request; (void)arg;
    /* TODO: Implement echo toggling, raw mode, etc. */
    return 0;
}

fs_node_t* tty_create_node(void) {
    fs_node_t* tty = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!tty) return NULL;

    memset(tty, 0, sizeof(fs_node_t));
    strlcpy(tty->name, "tty", 128);
    tty->flags = FS_CHARDEVICE;
    tty->inode = 0; /* Device ID? */
    tty->length = 0;
    tty->read = tty_read;
    tty->write = tty_write;
    tty->open = NULL;
    tty->close = NULL;
    tty->ioctl = tty_ioctl;
    tty->ref_count = 1;
    tty->lock = 0;

    return tty;
}
