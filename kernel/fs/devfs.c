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
#include <gfx/drm.h>
#include <serial.h>
#include <termios.h>
#include <task.h>
#include <errno.h>
#include <sys/signal.h>

/* Global root node for DevFS */
static fs_node_t* devfs_root = NULL;
static uint32_t dev_mouse_read_debug = 0;
static struct termios dev_tty_termios = {0};
static struct winsize dev_tty_winsz = {25, 80, 0, 0};
static int dev_tty_session = -1;
static int dev_tty_fg_pgid = 0;
static spinlock_t dev_tty_lock = 0;

static void dev_signal_pgrp(int sid, int pgid, int sig) {
    if (sig < 1 || sig > 31 || pgid <= 0) return;

    int max = task_get_max();
    for (int i = 0; i < max; i++) {
        task_t* t = task_get_at(i);
        if (!t) continue;
        if (t->state == TASK_DEAD) continue;
        if ((int)t->sid != sid) continue;
        if ((int)t->pgid != pgid) continue;
        t->signal_pending |= (1u << (sig - 1));
        if (t->state == TASK_BLOCKED || t->state == TASK_SLEEPING || t->state == TASK_STOPPED) {
            task_wakeup(t);
        }
    }
}

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
/* ========================================================================= */
/* /dev/binder Implementation (Basic Android IPC Routing)                      */
/* ========================================================================= */
#define BINDER_VERSION_IOWR (int)0xc0046209 /* Linux ioctl code for BINDER_VERSION */
#define BINDER_WRITE_READ (int)0xc0306201
#define BINDER_SET_CONTEXT_MGR (int)0x40046207

struct binder_version {
    int32_t protocol_version;
};

struct binder_write_read {
    uint64_t write_size;
    uint64_t write_consumed;
    uint64_t write_buffer;
    uint64_t read_size;
    uint64_t read_consumed;
    uint64_t read_buffer;
};

struct binder_transaction_data {
    union {
        uint32_t handle;
        void *ptr;
    } target;
    void *cookie;
    uint32_t code;
    uint32_t flags;
    int32_t sender_pid;
    int32_t sender_euid;
    uint64_t data_size;
    uint64_t offsets_size;
    union {
        struct {
            uint64_t buffer;
            uint64_t offsets;
        } ptr;
        uint8_t buf[8];
    } data;
};

enum {
    BC_TRANSACTION = 0x40406300,
    BC_REPLY       = 0x40406301,
    BC_FREE_BUFFER = 0x40406303,
};

enum {
    BR_ERROR = 0x80047200,
    BR_OK    = 0x7201,
    BR_TRANSACTION = 0x80287202,
    BR_REPLY       = 0x80287203,
    BR_DEAD_REPLY  = 0x7205,
    BR_TRANSACTION_COMPLETE = 0x7206,
    BR_NOOP        = 0x720c,
};

/* Basic state for routing */
static int binder_context_mgr_pid = -1;
static spinlock_t binder_lock = 0;

/* Simple queue to hold transactions destined for context manager */
#define BINDER_MAX_QUEUED 16
static struct binder_transaction_data cm_queue[BINDER_MAX_QUEUED];
static uint8_t *payload_buffers[BINDER_MAX_QUEUED];
static int cm_q_head = 0;
static int cm_q_tail = 0;

/* Client reply queues */
typedef struct {
    int pid;
    struct binder_transaction_data tr;
    uint8_t *payload;
    int used;
} client_queue_t;

static client_queue_t cl_queues[BINDER_MAX_QUEUED];

/* Helper to safely copy from user space. In a real kernel this would handle page faults. */
static int safe_copy_from_user(void *dest, const void *src, size_t n) {
    if (!src || !dest) return -1;
    memcpy(dest, src, n);
    return 0;
}

/* Helper to safely copy to user space. */
static int safe_copy_to_user(void *dest, const void *src, size_t n) {
    if (!src || !dest) return -1;
    memcpy(dest, src, n);
    return 0;
}

static ssize_t dev_binder_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset; (void)size; (void)buffer;
    return 0;
}

static uint32_t dev_binder_write(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset; (void)buffer;
    return size;
}

static int dev_binder_ioctl(fs_node_t *node, int request, void *arg) {
    (void)node;
    task_t *current = task_get_current();
    if (!current) return -1;

    if (request == BINDER_VERSION_IOWR) {
        if (!arg) return -1;
        struct binder_version ver;
        ver.protocol_version = 8; /* Current Binder protocol version */
        if (safe_copy_to_user(arg, &ver, sizeof(ver)) != 0) return -1;
        return 0;
    }
    if (request == BINDER_SET_CONTEXT_MGR) {
        spin_lock(&binder_lock);
        if (binder_context_mgr_pid != -1 && binder_context_mgr_pid != (int)current->id) {
            spin_unlock(&binder_lock);
            return -1; /* Already set */
        }
        binder_context_mgr_pid = (int)current->id;
        spin_unlock(&binder_lock);
        return 0;
    }
    if (request == BINDER_WRITE_READ) {
        if (!arg) return -1;
        struct binder_write_read bwr;
        if (safe_copy_from_user(&bwr, arg, sizeof(bwr)) != 0) return -1;

        /* Process Writes (Client -> Binder) */
        if (bwr.write_size > 0 && bwr.write_buffer) {
            uint8_t *wbuf = (uint8_t*)(uintptr_t)bwr.write_buffer;
            uint64_t consumed = 0;

            while (consumed + sizeof(uint32_t) <= bwr.write_size) {
                uint32_t cmd;
                if (safe_copy_from_user(&cmd, wbuf + consumed, sizeof(cmd)) != 0) break;
                consumed += sizeof(uint32_t);

                if (cmd == BC_TRANSACTION || cmd == BC_REPLY) {
                    if (consumed + sizeof(struct binder_transaction_data) > bwr.write_size) break;
                    struct binder_transaction_data tr;
                    if (safe_copy_from_user(&tr, wbuf + consumed, sizeof(tr)) != 0) break;
                    consumed += sizeof(struct binder_transaction_data);

                    uint8_t *temp_payload = NULL;
                    if (tr.data.ptr.buffer && tr.data_size > 0) {
                        /* Real allocation instead of a static buffer, capped at 1MB to prevent OOM */
                        if (tr.data_size <= 1024 * 1024) {
                            temp_payload = (uint8_t*)kmalloc((size_t)tr.data_size);
                            if (temp_payload) {
                                if (safe_copy_from_user(temp_payload, (void*)(uintptr_t)tr.data.ptr.buffer, (size_t)tr.data_size) != 0) {
                                    kfree(temp_payload);
                                    temp_payload = NULL;
                                    tr.data_size = 0;
                                }
                            } else {
                                tr.data_size = 0;
                            }
                        } else {
                            tr.data_size = 0; /* Deny oversized requests */
                        }
                    }

                    spin_lock(&binder_lock);
                    if (tr.target.handle == 0 && cmd != BC_REPLY) {
                        /* Route to context manager */
                        int next_head = (cm_q_head + 1) % BINDER_MAX_QUEUED;
                        if (next_head != cm_q_tail) {
                            memcpy(&cm_queue[cm_q_head], &tr, sizeof(struct binder_transaction_data));
                            cm_queue[cm_q_head].sender_pid = current->id;

                            payload_buffers[cm_q_head] = temp_payload;
                            cm_q_head = next_head;
                        } else {
                            if (temp_payload) kfree(temp_payload);
                        }
                    } else if (cmd == BC_REPLY) {
                        /* Target is encoded in the transaction struct for replies, or handle explicitly maps to PID */
                        /* Assuming target pid is in sender_pid for simplicity in this basic IPC */
                        int target_pid = tr.target.handle;
                        int stored = 0;
                        for (int i = 0; i < BINDER_MAX_QUEUED; i++) {
                            if (!cl_queues[i].used) {
                                cl_queues[i].pid = target_pid;
                                memcpy(&cl_queues[i].tr, &tr, sizeof(struct binder_transaction_data));
                                cl_queues[i].payload = temp_payload;
                                cl_queues[i].used = 1;
                                stored = 1;
                                break;
                            }
                        }
                        if (!stored && temp_payload) {
                            kfree(temp_payload);
                        }
                    } else {
                        if (temp_payload) kfree(temp_payload);
                    }
                    spin_unlock(&binder_lock);

                } else if (cmd == BC_FREE_BUFFER) {
                     if (consumed + sizeof(uintptr_t) > bwr.write_size) break;
                     uintptr_t buffer_to_free;
                     if (safe_copy_from_user(&buffer_to_free, wbuf + consumed, sizeof(buffer_to_free)) != 0) break;
                     consumed += sizeof(uintptr_t);

                     /* We must validate that the buffer belongs to our tracked payloads to prevent arbitrary kfree */
                     spin_lock(&binder_lock);
                     for (int i = 0; i < BINDER_MAX_QUEUED; i++) {
                         if (payload_buffers[i] == (uint8_t*)buffer_to_free && buffer_to_free != 0) {
                             kfree((void*)buffer_to_free);
                             payload_buffers[i] = NULL;
                             break;
                         }
                         if (cl_queues[i].payload == (uint8_t*)buffer_to_free && buffer_to_free != 0) {
                             kfree((void*)buffer_to_free);
                             cl_queues[i].payload = NULL;
                             break;
                         }
                     }
                     spin_unlock(&binder_lock);

                } else {
                    /* Unknown or unimplemented command, break */
                    break;
                }
            }
            bwr.write_consumed = consumed;
        }

        /* Process Reads (Binder -> Client/Context Manager) */
        if (bwr.read_size > 0 && bwr.read_buffer) {
            uint8_t *rbuf = (uint8_t*)(uintptr_t)bwr.read_buffer;
            uint64_t read_out = 0;
            struct binder_transaction_data tr_copy;
            uint8_t *payload_ptr = NULL;
            int has_tr = 0;
            uint32_t out_cmd = BR_NOOP;

            spin_lock(&binder_lock);
            if (current->id == (uint32_t)binder_context_mgr_pid) {
                if (cm_q_head != cm_q_tail) {
                    memcpy(&tr_copy, &cm_queue[cm_q_tail], sizeof(struct binder_transaction_data));
                    payload_ptr = payload_buffers[cm_q_tail];
                    cm_q_tail = (cm_q_tail + 1) % BINDER_MAX_QUEUED;
                    has_tr = 1;
                    out_cmd = BR_TRANSACTION;
                }
            } else {
                for (int i = 0; i < BINDER_MAX_QUEUED; i++) {
                    if (cl_queues[i].used && cl_queues[i].pid == (int)current->id) {
                        memcpy(&tr_copy, &cl_queues[i].tr, sizeof(struct binder_transaction_data));
                        payload_ptr = cl_queues[i].payload;
                        cl_queues[i].used = 0;
                        has_tr = 1;
                        out_cmd = BR_REPLY;
                        break;
                    }
                }
            }
            spin_unlock(&binder_lock);

            if (has_tr) {
                if (bwr.read_size >= sizeof(uint32_t) + sizeof(struct binder_transaction_data)) {
                    /* Basic shim copies data to the user provided buffer instead of mmaps, to avoid passing kernel pointers */
                    if (payload_ptr && tr_copy.data.ptr.buffer) {
                        /* In this shim, we assume the reader provided a valid buffer in tr_copy.data.ptr.buffer
                           to copy the payload into. This is not strictly Android compliant but safe for our baremetal shim. */
                        if (safe_copy_to_user((void*)(uintptr_t)tr_copy.data.ptr.buffer, payload_ptr, tr_copy.data_size) != 0) {
                           /* Copy failed */
                        }
                        /* We don't free payload_ptr here, BC_FREE_BUFFER must handle it based on the tracking logic */
                    } else {
                        tr_copy.data.ptr.buffer = 0;
                    }

                    if (safe_copy_to_user(rbuf + read_out, &out_cmd, sizeof(out_cmd)) == 0) {
                        read_out += sizeof(uint32_t);
                        if (safe_copy_to_user(rbuf + read_out, &tr_copy, sizeof(tr_copy)) == 0) {
                            read_out += sizeof(struct binder_transaction_data);
                        }
                    }
                } else {
                    /* Not enough space, we leak the kernel buffer here for simplicity in this basic version, or we could queue it back.
                       We drop it to avoid infinite loops */
                }
            } else {
                if (bwr.read_size >= sizeof(uint32_t)) {
                    uint32_t noop = BR_NOOP;
                    if (safe_copy_to_user(rbuf + read_out, &noop, sizeof(noop)) == 0) {
                        read_out += sizeof(uint32_t);
                    }
                }
            }
            bwr.read_consumed = read_out;
        }

        if (safe_copy_to_user(arg, &bwr, sizeof(bwr)) != 0) return -1;
        return 0;
    }

    return -1; /* ENOTTY */
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
#include <drivers/ata.h>

/* ========================================================================= */
/* /dev/sda Implementation (Raw Disk)                                        */
/* ========================================================================= */
static ssize_t dev_sda_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node;
    if (!buffer) return 0;
    
    uint32_t start_sector = offset / 512;
    uint32_t sector_offset = offset % 512;
    uint32_t end_sector = (offset + size - 1) / 512;
    uint32_t total_sectors = end_sector - start_sector + 1;
    
    uint8_t sect_buf[512];
    uint32_t bytes_read = 0;
    
    for (uint32_t i = 0; i < total_sectors; i++) {
        uint32_t current_lba = start_sector + i;
        ata_read_sector(current_lba, sect_buf);
        
        uint32_t chunk_start = (i == 0) ? sector_offset : 0;
        uint32_t chunk_end = (i == total_sectors - 1) ? ((size - bytes_read < 512 - chunk_start) ? chunk_start + (size - bytes_read) : 512) : 512;
        uint32_t chunk_size = chunk_end - chunk_start;
        
        memcpy(buffer + bytes_read, sect_buf + chunk_start, chunk_size);
        bytes_read += chunk_size;
    }
    
    return bytes_read;
}

static uint32_t dev_sda_write(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node;
    if (!buffer) return 0;
    
    uint32_t start_sector = offset / 512;
    uint32_t sector_offset = offset % 512;
    uint32_t end_sector = (offset + size - 1) / 512;
    uint32_t total_sectors = end_sector - start_sector + 1;
    
    uint8_t sect_buf[512];
    uint32_t bytes_written = 0;
    
    for (uint32_t i = 0; i < total_sectors; i++) {
        uint32_t current_lba = start_sector + i;
        uint32_t chunk_start = (i == 0) ? sector_offset : 0;
        uint32_t chunk_end = (i == total_sectors - 1) ? ((size - bytes_written < 512 - chunk_start) ? chunk_start + (size - bytes_written) : 512) : 512;
        uint32_t chunk_size = chunk_end - chunk_start;
        
        if (chunk_size < 512) {
            ata_read_sector(current_lba, sect_buf);
        }
        
        memcpy(sect_buf + chunk_start, buffer + bytes_written, chunk_size);
        ata_write_sector(current_lba, sect_buf);
        bytes_written += chunk_size;
    }
    
    return bytes_written;
}
static ssize_t dev_tty_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset;
    task_t* current = task_get_current();
    if (!current) return -EINVAL;

    spin_lock(&dev_tty_lock);
    if (dev_tty_session != -1 &&
        (int)current->sid == dev_tty_session &&
        dev_tty_fg_pgid > 0 &&
        (int)current->pgid != dev_tty_fg_pgid) {
        int sid = dev_tty_session;
        int pg = (int)current->pgid;
        spin_unlock(&dev_tty_lock);
        dev_signal_pgrp(sid, pg, SIGTTIN);
        return -EIO;
    }
    int isig = (dev_tty_termios.c_lflag & ISIG) ? 1 : 0;
    int sid = dev_tty_session;
    int fg = dev_tty_fg_pgid;
    spin_unlock(&dev_tty_lock);

    for (uint32_t i = 0; i < size; i++) {
        uint8_t c = (uint8_t)keyboard_getchar();
        if (isig && (c == 0x03 || c == 0x1A)) { /* Ctrl-C / Ctrl-Z */
            if (sid != -1 && fg > 0) {
                dev_signal_pgrp(sid, fg, (c == 0x03) ? SIGINT : SIGTSTP);
            }
            continue;
        }
        buffer[i] = c;
        /* Line buffered simulation (optional) */
        if (buffer[i] == '\n') return i+1;
    }
    return size;
}

static uint32_t dev_tty_write(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset;
    task_t* current = task_get_current();
    if (!current) return 0;

    spin_lock(&dev_tty_lock);
    if (dev_tty_session != -1 &&
        (int)current->sid == dev_tty_session &&
        dev_tty_fg_pgid > 0 &&
        (int)current->pgid != dev_tty_fg_pgid) {
        int sid = dev_tty_session;
        int pg = (int)current->pgid;
        spin_unlock(&dev_tty_lock);
        dev_signal_pgrp(sid, pg, SIGTTOU);
        return 0;
    }
    spin_unlock(&dev_tty_lock);

    for (uint32_t i = 0; i < size; i++) {
        terminal_putchar((char)buffer[i]);
    }
    return size;
}

static int dev_tty_ioctl(fs_node_t *node, int request, void *arg) {
    (void)node;
    task_t* current = task_get_current();
    if (!current) return -EINVAL;

    spin_lock(&dev_tty_lock);
    switch (request) {
        case TCGETS:
            memcpy(arg, &dev_tty_termios, sizeof(struct termios));
            spin_unlock(&dev_tty_lock);
            return 0;
        case TCSETS:
            memcpy(&dev_tty_termios, arg, sizeof(struct termios));
            spin_unlock(&dev_tty_lock);
            return 0;
        case TIOCGWINSZ:
            memcpy(arg, &dev_tty_winsz, sizeof(struct winsize));
            spin_unlock(&dev_tty_lock);
            return 0;
        case TIOCSWINSZ:
            memcpy(&dev_tty_winsz, arg, sizeof(struct winsize));
            spin_unlock(&dev_tty_lock);
            return 0;
        case TIOCSCTTY:
            if (current->sid == 0) {
                spin_unlock(&dev_tty_lock);
                return -EPERM;
            }
            if (dev_tty_session != -1 && dev_tty_session != (int)current->sid) {
                spin_unlock(&dev_tty_lock);
                return -EPERM;
            }
            dev_tty_session = (int)current->sid;
            if (dev_tty_fg_pgid == 0) dev_tty_fg_pgid = (int)current->pgid;
            spin_unlock(&dev_tty_lock);
            return 0;
        case TIOCNOTTY:
            if (dev_tty_session == (int)current->sid) {
                dev_tty_session = -1;
                dev_tty_fg_pgid = 0;
            }
            spin_unlock(&dev_tty_lock);
            return 0;
        case TIOCGPGRP:
            if (!arg) { spin_unlock(&dev_tty_lock); return -EINVAL; }
            *(int*)arg = dev_tty_fg_pgid;
            spin_unlock(&dev_tty_lock);
            return 0;
        case TIOCSPGRP: {
            if (!arg) { spin_unlock(&dev_tty_lock); return -EINVAL; }
            int pgid = *(int*)arg;
            if (pgid <= 0 || dev_tty_session != (int)current->sid) {
                spin_unlock(&dev_tty_lock);
                return -EPERM;
            }
            dev_tty_fg_pgid = pgid;
            spin_unlock(&dev_tty_lock);
            return 0;
        }
        case FIONREAD:
            if (!arg) { spin_unlock(&dev_tty_lock); return -EINVAL; }
            *(int*)arg = keyboard_has_input() ? 1 : 0;
            spin_unlock(&dev_tty_lock);
            return 0;
        default:
            spin_unlock(&dev_tty_lock);
            return -EINVAL;
    }
}

/* ========================================================================= */
/* /dev/ptmx and /dev/pts (minimal PTY implementation)                       */
/* ========================================================================= */
#define MAX_PTYS 16
#define PTY_BUF_SIZE 4096
#define PTY_INODE_BASE 2000
#define PTY_ROLE_MASTER 0
#define PTY_ROLE_SLAVE  1

typedef struct {
    uint8_t data[PTY_BUF_SIZE];
    uint32_t rpos;
    uint32_t wpos;
    uint32_t avail;
} pty_ring_t;

typedef struct {
    int used;
    int locked;
    uint32_t id;
    int master_refs;
    int slave_refs;
    int session_id;
    int fg_pgid;
    struct termios termios_state;
    struct winsize winsz;
    pty_ring_t m2s;
    pty_ring_t s2m;
    spinlock_t lock;
} pty_pair_t;

static pty_pair_t g_ptys[MAX_PTYS];
static spinlock_t g_ptys_lock = 0;

static pty_pair_t* pty_from_node(fs_node_t *node) {
    if (!node) return NULL;
    uint32_t id = node->impl;
    if (id >= MAX_PTYS) return NULL;
    if (!g_ptys[id].used) return NULL;
    return &g_ptys[id];
}

static int pty_node_role(fs_node_t *node) {
    if (!node) return PTY_ROLE_MASTER;
    return (node->inode & 1) ? PTY_ROLE_SLAVE : PTY_ROLE_MASTER;
}

static ssize_t pty_ring_read(pty_pair_t* p, pty_ring_t* r, uint8_t *buffer, uint32_t size, int writer_refs) {
    uint32_t read = 0;
    if (!p || !r || !buffer) return 0;

    while (read < size) {
        spin_lock(&p->lock);
        if (r->avail == 0) {
            spin_unlock(&p->lock);
            if (writer_refs <= 0) break; /* EOF */
            if (read > 0) break;
            task_yield();
            continue;
        }
        while (read < size && r->avail > 0) {
            buffer[read++] = r->data[r->rpos];
            r->rpos = (r->rpos + 1) % PTY_BUF_SIZE;
            r->avail--;
        }
        spin_unlock(&p->lock);
    }
    return read;
}

static uint32_t pty_ring_write(pty_pair_t* p, pty_ring_t* r, uint8_t *buffer, uint32_t size, int reader_refs) {
    uint32_t written = 0;
    if (!p || !r || !buffer) return 0;
    if (reader_refs <= 0) return 0;

    while (written < size) {
        spin_lock(&p->lock);
        if (r->avail == PTY_BUF_SIZE) {
            spin_unlock(&p->lock);
            if (written > 0) break;
            task_yield();
            continue;
        }
        while (written < size && r->avail < PTY_BUF_SIZE) {
            r->data[r->wpos] = buffer[written++];
            r->wpos = (r->wpos + 1) % PTY_BUF_SIZE;
            r->avail++;
        }
        spin_unlock(&p->lock);
    }
    return written;
}

static ssize_t dev_pty_read(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)offset;
    pty_pair_t* p = pty_from_node(node);
    int role = pty_node_role(node);
    task_t* current = task_get_current();
    if (!p) return 0;
    if (!current) return -EINVAL;

    if (role == PTY_ROLE_MASTER) {
        return pty_ring_read(p, &p->s2m, buffer, size, p->slave_refs);
    }
    if (p->session_id != -1 &&
        (int)current->sid == p->session_id &&
        p->fg_pgid > 0 &&
        (int)current->pgid != p->fg_pgid) {
        dev_signal_pgrp(p->session_id, (int)current->pgid, SIGTTIN);
        return -EIO;
    }
    return pty_ring_read(p, &p->m2s, buffer, size, p->master_refs);
}

static uint32_t dev_pty_write(fs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)offset;
    pty_pair_t* p = pty_from_node(node);
    int role = pty_node_role(node);
    task_t* current = task_get_current();
    if (!p) return 0;
    if (!current) return 0;

    if (role == PTY_ROLE_MASTER) {
        uint32_t written = 0;
        int isig = (p->termios_state.c_lflag & ISIG) ? 1 : 0;
        for (uint32_t i = 0; i < size; i++) {
            uint8_t c = buffer[i];
            if (isig && (c == 0x03 || c == 0x1A)) { /* Ctrl-C / Ctrl-Z */
                if (p->session_id != -1 && p->fg_pgid > 0) {
                    dev_signal_pgrp(p->session_id, p->fg_pgid, (c == 0x03) ? SIGINT : SIGTSTP);
                }
                continue;
            }
            written += pty_ring_write(p, &p->m2s, &c, 1, p->slave_refs);
        }
        return written;
    }
    if (p->session_id != -1 &&
        (int)current->sid == p->session_id &&
        p->fg_pgid > 0 &&
        (int)current->pgid != p->fg_pgid) {
        dev_signal_pgrp(p->session_id, (int)current->pgid, SIGTTOU);
        return 0;
    }
    return pty_ring_write(p, &p->s2m, buffer, size, p->master_refs);
}

static void dev_pty_close(fs_node_t *node) {
    pty_pair_t* p = pty_from_node(node);
    int role = pty_node_role(node);
    if (!p) return;

    spin_lock(&g_ptys_lock);
    if (role == PTY_ROLE_MASTER) {
        if (p->master_refs > 0) p->master_refs--;
    } else {
        if (p->slave_refs > 0) p->slave_refs--;
    }
    if (p->master_refs == 0 && p->slave_refs == 0) {
        memset(p, 0, sizeof(*p));
    }
    spin_unlock(&g_ptys_lock);
}

static void dev_pty_open(fs_node_t *node) {
    pty_pair_t* p = pty_from_node(node);
    int role = pty_node_role(node);
    if (!p) return;

    spin_lock(&g_ptys_lock);
    if (role == PTY_ROLE_MASTER) p->master_refs++;
    else p->slave_refs++;
    spin_unlock(&g_ptys_lock);
}

static int dev_pty_ioctl(fs_node_t *node, int request, void *arg) {
    pty_pair_t* p = pty_from_node(node);
    int role = pty_node_role(node);
    task_t* current = task_get_current();
    if (!p) return -EINVAL;
    if (!arg && request != FIONREAD) return -EINVAL;
    if (!current) return -EINVAL;

    switch (request) {
        case TCGETS:
            memcpy(arg, &p->termios_state, sizeof(struct termios));
            return 0;
        case TCSETS:
            memcpy(&p->termios_state, arg, sizeof(struct termios));
            return 0;
        case TIOCGWINSZ:
            memcpy(arg, &p->winsz, sizeof(struct winsize));
            return 0;
        case TIOCSWINSZ:
            memcpy(&p->winsz, arg, sizeof(struct winsize));
            return 0;
        case TIOCGPTN:
            if (role != PTY_ROLE_MASTER) return -EINVAL;
            *(int*)arg = (int)p->id;
            return 0;
        case TIOCSPTLCK:
            if (role != PTY_ROLE_MASTER) return -EINVAL;
            p->locked = (*(int*)arg) ? 1 : 0;
            return 0;
        case TIOCSCTTY:
            if (role != PTY_ROLE_SLAVE) return -EINVAL;
            if (current->sid == 0) return -EPERM;
            if (p->session_id != -1 && p->session_id != (int)current->sid) return -EPERM;
            p->session_id = (int)current->sid;
            if (p->fg_pgid == 0) p->fg_pgid = (int)current->pgid;
            return 0;
        case TIOCNOTTY:
            if (p->session_id == (int)current->sid) {
                p->session_id = -1;
                p->fg_pgid = 0;
            }
            return 0;
        case TIOCGPGRP:
            if (!arg) return -EINVAL;
            *(int*)arg = p->fg_pgid;
            return 0;
        case TIOCSPGRP: {
            if (!arg) return -EINVAL;
            int pgid = *(int*)arg;
            if (pgid <= 0) return -EINVAL;
            if (p->session_id != (int)current->sid) return -EPERM;
            p->fg_pgid = pgid;
            return 0;
        }
        case FIONREAD:
            if (!arg) return -EINVAL;
            if (role == PTY_ROLE_MASTER) *(int*)arg = (int)p->s2m.avail;
            else *(int*)arg = (int)p->m2s.avail;
            return 0;
        default:
            return -EINVAL;
    }
}

static void dev_ptmx_open(fs_node_t *node) {
    int id = -1;
    spin_lock(&g_ptys_lock);
    for (int i = 0; i < MAX_PTYS; i++) {
        if (!g_ptys[i].used) {
            id = i;
            memset(&g_ptys[i], 0, sizeof(g_ptys[i]));
            g_ptys[i].used = 1;
            g_ptys[i].id = (uint32_t)i;
            g_ptys[i].locked = 0;
            g_ptys[i].master_refs = 1;
            g_ptys[i].session_id = -1;
            g_ptys[i].fg_pgid = 0;
            g_ptys[i].termios_state.c_lflag = ECHO | ICANON | ISIG;
            g_ptys[i].winsz.ws_row = 25;
            g_ptys[i].winsz.ws_col = 80;
            break;
        }
    }
    spin_unlock(&g_ptys_lock);

    if (id < 0) {
        node->impl = 0xFFFFFFFFu;
        return;
    }

    node->impl = (uint32_t)id;
    node->inode = PTY_INODE_BASE + ((uint32_t)id * 2u) + PTY_ROLE_MASTER;
    node->read = dev_pty_read;
    node->write = dev_pty_write;
    node->ioctl = dev_pty_ioctl;
    node->close = dev_pty_close;
    node->open = dev_pty_open;
}

static int devfs_pts_readdir(fs_node_t *node, uint32_t index, struct dirent *entry) {
    (void)node;
    uint32_t seen = 0;
    for (uint32_t i = 0; i < MAX_PTYS; i++) {
        if (!g_ptys[i].used) continue;
        if (seen == index) {
            char num_buf[16];
            itoa_s((int64_t)i, num_buf, sizeof(num_buf), 10);
            strlcpy(entry->name, num_buf, sizeof(entry->name));
            entry->inode = PTY_INODE_BASE + (i * 2u) + PTY_ROLE_SLAVE;
            return 0;
        }
        seen++;
    }
    return 1;
}

static fs_node_t *devfs_pts_finddir(fs_node_t *node, char *name) {
    (void)node;
    uint32_t id = 0;
    if (!name || name[0] == '\0') return 0;
    for (int i = 0; name[i]; i++) {
        if (name[i] < '0' || name[i] > '9') return 0;
        id = id * 10u + (uint32_t)(name[i] - '0');
    }
    if (id >= MAX_PTYS) return 0;
    if (!g_ptys[id].used || g_ptys[id].locked) return 0;

    fs_node_t *fnode = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!fnode) return 0;
    memset(fnode, 0, sizeof(fs_node_t));
    fnode->ref_count = 1;
    fnode->flags = FS_CHARDEVICE;
    fnode->mask = 0666;
    strlcpy(fnode->name, name, sizeof(fnode->name));
    fnode->inode = PTY_INODE_BASE + (id * 2u) + PTY_ROLE_SLAVE;
    fnode->impl = id;
    fnode->read = dev_pty_read;
    fnode->write = dev_pty_write;
    fnode->ioctl = dev_pty_ioctl;
    fnode->open = dev_pty_open;
    fnode->close = dev_pty_close;
    return fnode;
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
    fnode->mask = 0666;

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
    if (index == 8) {
        strlcpy(entry->name, "ptmx", sizeof(entry->name));
        entry->inode = 8;
        return 0;
    }
    if (index == 9) {
        strlcpy(entry->name, "pts", sizeof(entry->name));
        entry->inode = 9;
        return 0;
    }
    if (index == 10) {
        strlcpy(entry->name, "sda", sizeof(entry->name));
        entry->inode = 10;
        return 0;
    }
    if (index == 11) {
        strlcpy(entry->name, "dri", sizeof(entry->name));
        entry->inode = 11;
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
    fnode->mask = 0666;

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
        fnode->ioctl = dev_tty_ioctl;
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
        fnode->mask = 0555;
        fnode->readdir = devfs_input_readdir;
        fnode->finddir = devfs_input_finddir;
        fnode->read = NULL;
        fnode->inode = 5;
    } else if (strcmp(name, "fb0") == 0) {
        strlcpy(fnode->name, "fb0", sizeof(fnode->name));
        fnode->flags = FS_CHARDEVICE;
    fnode->mask = 0666;
        fnode->ioctl = dev_fb0_ioctl;
        fnode->inode = 6;
    } else if (strcmp(name, "binder") == 0) {
        strlcpy(fnode->name, "binder", sizeof(fnode->name));
        fnode->flags = FS_CHARDEVICE;
    fnode->mask = 0666;
        fnode->read = dev_binder_read;
        fnode->write = dev_binder_write;
        fnode->ioctl = dev_binder_ioctl;
        fnode->inode = 7;
    } else if (strcmp(name, "shm") == 0) {
        strlcpy(fnode->name, "shm", sizeof(fnode->name));
        fnode->flags = FS_DIRECTORY;
        fnode->mask = 0777;
        fnode->inode = 7;
    } else if (strcmp(name, "ptmx") == 0) {
        strlcpy(fnode->name, "ptmx", sizeof(fnode->name));
        fnode->flags = FS_CHARDEVICE;
    fnode->mask = 0666;
        fnode->inode = 8;
        fnode->open = dev_ptmx_open;
    } else if (strcmp(name, "pts") == 0) {
        strlcpy(fnode->name, "pts", sizeof(fnode->name));
        fnode->flags = FS_DIRECTORY;
        fnode->mask = 0555;
        fnode->inode = 9;
        fnode->readdir = devfs_pts_readdir;
        fnode->finddir = devfs_pts_finddir;
    } else if (strcmp(name, "sda") == 0) {
        strlcpy(fnode->name, "sda", sizeof(fnode->name));
        fnode->flags = FS_BLOCKDEVICE;
        fnode->mask = 0600; // Solo root puede escribir el disco crudo
        fnode->inode = 10;
        fnode->read = dev_sda_read;
        fnode->write = dev_sda_write;    } else if (strcmp(name, "dri") == 0) {
        strlcpy(fnode->name, "dri", sizeof(fnode->name));
        fnode->flags = FS_DIRECTORY;
        fnode->mask = 0555;
        fnode->inode = 11;
        fnode->readdir = devfs_dri_readdir;
        fnode->finddir = devfs_dri_finddir;
} else {
        kfree(fnode);
        return 0;
    }
    return fnode;
}

static int devfs_create(fs_node_t *parent, char *name, uint16_t permission) {
    (void)parent; (void)name; (void)permission;
    return -EROFS;
}

static int devfs_mkdir(fs_node_t *parent, char *name, uint16_t permission) {
    (void)parent; (void)name; (void)permission;
    return -EROFS;
}

fs_node_t* devfs_init(void) {
    devfs_root = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!devfs_root) return NULL;

    memset(devfs_root, 0, sizeof(fs_node_t));
    devfs_root->ref_count = 1;
    strlcpy(devfs_root->name, "dev", sizeof(devfs_root->name));
    devfs_root->flags = FS_DIRECTORY;
    devfs_root->mask = 0555;
    devfs_root->readdir = devfs_readdir;
    devfs_root->finddir = devfs_finddir;
    devfs_root->create = devfs_create;
    devfs_root->mkdir = devfs_mkdir;
    dev_tty_termios.c_lflag = ECHO | ICANON | ISIG;

    return devfs_root;
}
