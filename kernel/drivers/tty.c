#include <fs/vfs.h>
#include <keyboard.h>
#include <vga.h>
#include <serial.h>
#include <mm.h>
#include <string.h>
#include <types.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <ioctl.h>

/* TTY Device Structure */
typedef struct {
    struct termios termios_state;
} tty_struct_t;

/* TTY Node Functions */

static ssize_t tty_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)offset;
    if (!node || !node->ptr) return -EINVAL;

    tty_struct_t *tty_state = (tty_struct_t *)node->ptr;

    if (size == 0) return 0;

    /* Read from keyboard buffer */
    /* This blocks until input is available */
    uint32_t i = 0;
    while (i < size) {
        if ((node->flags & O_NONBLOCK) && !keyboard_has_input()) {
            if (i == 0) return -EAGAIN;
            return i;
        }
        char c = keyboard_getchar();

        /* Echo to screen and serial */
        if (tty_state->termios_state.c_lflag & ECHO) {
            terminal_putchar(c);
            serial_putchar(c);
        }

        if (tty_state->termios_state.c_lflag & ICANON) {
            /* Handle Backspace */
            if (c == '\b') {
                if (i > 0) {
                    i--;
                    if (tty_state->termios_state.c_lflag & ECHO) {
                        /* Erase from screen: Backspace + Space + Backspace */
                        terminal_putchar(' ');
                        terminal_putchar('\b');
                        /* Serial backspace */
                        serial_putchar(' ');
                        serial_putchar('\b');
                    }
                }
                continue;
            }

            buffer[i++] = c;
            if (c == '\n' || c == '\r') {
                /* Normalize newline */
                if (c == '\r') buffer[i-1] = '\n';
                return i;
            }
        } else {
            /* Raw mode: no backspace processing, return immediately */
            buffer[i++] = c;
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
    if (!node || !node->ptr) return -EINVAL;
    if (!arg) return -EINVAL;

    tty_struct_t *tty_state = (tty_struct_t *)node->ptr;

    switch (request) {
        case TCGETS: {
            struct termios *t = (struct termios *)arg;
            memcpy(t, &tty_state->termios_state, sizeof(struct termios));
            return 0;
        }
        case TCSETS: {
            struct termios *t = (struct termios *)arg;
            memcpy(&tty_state->termios_state, t, sizeof(struct termios));
            return 0;
        }
        case FIONREAD: {
            int *bytes = (int *)arg;
            *bytes = keyboard_has_input() ? 1 : 0;
            return 0;
        }
        default:
            return -EINVAL;
    }
}

static void tty_close(fs_node_t *node) {
    if (node && node->ptr) {
        kfree(node->ptr);
        node->ptr = NULL;
    }
}

fs_node_t* tty_create_node(void) {
    fs_node_t* tty = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!tty) return NULL;

    tty_struct_t* tty_state = (tty_struct_t*)kmalloc(sizeof(tty_struct_t));
    if (!tty_state) {
        kfree(tty);
        return NULL;
    }

    memset(tty_state, 0, sizeof(tty_struct_t));
    tty_state->termios_state.c_lflag = ECHO | ICANON | ISIG;

    memset(tty, 0, sizeof(fs_node_t));
    strlcpy(tty->name, "tty", 128);
    tty->flags = FS_CHARDEVICE;
    tty->inode = 0; /* Device ID? */
    tty->length = 0;
    tty->read = tty_read;
    tty->write = tty_write;
    tty->open = NULL;
    tty->close = tty_close;
    tty->ioctl = tty_ioctl;
    tty->ref_count = 1;
    tty->lock = 0;
    tty->ptr = (struct fs_node *)tty_state;

    return tty;
}
