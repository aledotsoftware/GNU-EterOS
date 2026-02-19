/**
 * éterOS - Kernel Logging System Implementation
 *
 * Centralized logging mechanism with severity levels and automatic routing.
 */

#include <klog.h>
#include <hal.h>
#include <stdarg.h>
#include <string.h>

/* ========================================================================= */
/* Configuration                                                             */
/* ========================================================================= */

#define KLOG_BUFFER_SIZE 1024

static char klog_buffer[KLOG_BUFFER_SIZE];
static volatile int klog_lock = 0;

/* ========================================================================= */
/* Helper: Spinlock                                                          */
/* ========================================================================= */

static void acquire_lock(void) {
    while (__sync_lock_test_and_set(&klog_lock, 1)) {
        /* Busy wait / pause */
        #ifdef __x86_64__
        __asm__ volatile("pause");
        #endif
    }
}

static void release_lock(void) {
    __sync_lock_release(&klog_lock);
}

/* ========================================================================= */
/* Helper: Integer to String (Internal)                                      */
/* ========================================================================= */

static char* klog_itoa(int value, char* str, int base) {
    char* rc;
    char* ptr;
    char* low;
    /* Check for supported base. */
    if (base < 2 || base > 36) {
        *str = '\0';
        return str;
    }
    rc = ptr = str;
    /* Set '-' for negative decimals. */
    if (value < 0 && base == 10) {
        *ptr++ = '-';
    }
    /* Remember where the numbers start. */
    low = ptr;
    /* The actual conversion. */
    /* Use unsigned to handle INT_MIN correctly */
    unsigned int num = (value < 0 && base == 10) ? -value : (unsigned int)value;

    do {
        *ptr++ = "0123456789abcdefghijklmnopqrstuvwxyz"[num % base];
        num /= base;
    } while (num);
    /* Terminate the string. */
    *ptr = '\0';
    /* Invert the numbers. */
    char* start = low;
    char* end = ptr - 1;
    while (start < end) {
        char tmp = *start;
        *start++ = *end;
        *end-- = tmp;
    }
    return rc;
}

static char* klog_utoa(unsigned int value, char* str, int base) {
    char* rc;
    char* ptr;
    char* low;
    if (base < 2 || base > 36) {
        *str = '\0';
        return str;
    }
    rc = ptr = str;
    low = ptr;
    do {
        *ptr++ = "0123456789abcdefghijklmnopqrstuvwxyz"[value % base];
        value /= base;
    } while (value);
    *ptr = '\0';
    char* start = low;
    char* end = ptr - 1;
    while (start < end) {
        char tmp = *start;
        *start++ = *end;
        *end-- = tmp;
    }
    return rc;
}

static char* klog_utoa_hex(uint64_t value, char* str) {
    char* ptr = str;
    int i;
    int found_nonzero = 0;

    *ptr++ = '0';
    *ptr++ = 'x';

    for (i = 15; i >= 0; i--) {
        int nibble = (value >> (i * 4)) & 0xF;
        if (nibble != 0 || found_nonzero || i == 0) {
            found_nonzero = 1;
            *ptr++ = "0123456789ABCDEF"[nibble];
        }
    }
    *ptr = '\0';
    return str;
}

/* ========================================================================= */
/* Helper: vsnprintf (Simplified)                                            */
/* ========================================================================= */

static void klog_vsnprintf(char* buffer, size_t size, const char* fmt, va_list args) {
    size_t i = 0;
    const char* p;

    for (p = fmt; *p != '\0' && i < size - 1; p++) {
        if (*p != '%') {
            buffer[i++] = *p;
            continue;
        }

        p++;
        switch (*p) {
            case 's': {
                const char* s = va_arg(args, const char*);
                if (!s) s = "(null)";
                while (*s && i < size - 1) {
                    buffer[i++] = *s++;
                }
                break;
            }
            case 'd': {
                int d = va_arg(args, int);
                char num_buf[32];
                klog_itoa(d, num_buf, 10);
                char* s = num_buf;
                while (*s && i < size - 1) {
                    buffer[i++] = *s++;
                }
                break;
            }
            case 'u': {
                unsigned int u = va_arg(args, unsigned int);
                char num_buf[32];
                klog_utoa(u, num_buf, 10);
                char* s = num_buf;
                while (*s && i < size - 1) {
                    buffer[i++] = *s++;
                }
                break;
            }
            case 'x': {
                unsigned int x = va_arg(args, unsigned int);
                char num_buf[32];
                klog_utoa(x, num_buf, 16);
                char* s = num_buf;
                while (*s && i < size - 1) {
                    buffer[i++] = *s++;
                }
                break;
            }
            case 'p': {
                void* ptr = va_arg(args, void*);
                char num_buf[32];
                klog_utoa_hex((uintptr_t)ptr, num_buf);
                char* s = num_buf;
                while (*s && i < size - 1) {
                    buffer[i++] = *s++;
                }
                break;
            }
            case 'c': {
                char c = (char)va_arg(args, int);
                if (i < size - 1) buffer[i++] = c;
                break;
            }
            case '%': {
                if (i < size - 1) buffer[i++] = '%';
                break;
            }
            default: {
                if (i < size - 1) buffer[i++] = '%';
                if (i < size - 1) buffer[i++] = *p;
                break;
            }
        }
    }
    buffer[i] = '\0';
}

/* ========================================================================= */
/* Implementation                                                            */
/* ========================================================================= */

void klog(int level, const char* fmt, ...) {
    /*
     * Disable interrupts to prevent deadlock if called from ISR.
     * Note: This unconditionally re-enables interrupts at the end, which might
     * be incorrect if they were already disabled (e.g. inside an ISR).
     * Since HAL doesn't support save/restore flags yet, this is best effort.
     */
    hal_interrupts_disable();
    acquire_lock();

    /* 1. Add Prefix */
    const char* prefix = "";
    switch (level) {
        case KLOG_DEBUG: prefix = "[DEBUG] "; break;
        case KLOG_INFO:  prefix = "[INFO ] "; break;
        case KLOG_WARN:  prefix = "[WARN ] "; break;
        case KLOG_ERROR: prefix = "[ERROR] "; break;
        case KLOG_PANIC: prefix = "[PANIC] "; break;
        default:         prefix = "[UNK  ] "; break;
    }

    size_t prefix_len = strlen(prefix);
    if (prefix_len < KLOG_BUFFER_SIZE) {
        /* Avoid strlcpy/strncpy dependency if possible, or use simplified copy */
        char* d = klog_buffer;
        const char* s = prefix;
        while (*s) *d++ = *s++;
        *d = '\0';
    }

    /* 2. Format Message */
    va_list args;
    va_start(args, fmt);
    /* Calculate remaining space */
    size_t current_len = prefix_len;
    if (current_len < KLOG_BUFFER_SIZE) {
        klog_vsnprintf(klog_buffer + current_len, KLOG_BUFFER_SIZE - current_len, fmt, args);
    }
    va_end(args);

    /* 3. Route Output */
    /* Always log to debug port (serial) for persistence */
    hal_debug_write(klog_buffer);

    if (level >= KLOG_INFO) {
        /* Info/Warn/Error/Panic -> Console (VGA) + Serial */
        /* If hal_console_write also writes to serial, we might get duplicates,
           but this ensures we don't miss logs on console-less systems. */
        hal_console_write(klog_buffer);
    }

    release_lock();
    hal_interrupts_enable();

    /* 4. Handle Panic */
    if (level == KLOG_PANIC) {
        /* Disable interrupts and halt */
        hal_interrupts_disable();
        while (1) {
            hal_cpu_halt();
        }
    }
}
