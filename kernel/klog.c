/**
 * éterOS - Kernel Logging System Implementation
 *
 * Centralized logging mechanism with severity levels and automatic routing.
 */

#include <klog.h>
#include <hal.h>
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

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
        vsnprintf(klog_buffer + current_len, KLOG_BUFFER_SIZE - current_len, fmt, args);
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
