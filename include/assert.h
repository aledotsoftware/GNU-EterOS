#ifndef ASSERT_H
#define ASSERT_H

#include <serial.h>

#define ASSERT(condition) \
    do { \
        if (!(condition)) { \
            serial_write_string("[PANIC] Assertion failed: " #condition " in " __FILE__ "\n"); \
            __asm__ volatile("cli; hlt"); \
        } \
    } while (0)

#endif
