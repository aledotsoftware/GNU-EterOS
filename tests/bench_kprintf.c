#define __ETEROS_HOST_TEST__
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <stdarg.h>

#include "../include/types.h"

#define memcpy eteros_memcpy
#define memset eteros_memset
#define memmove eteros_memmove
#define memcmp eteros_memcmp
#define strlen eteros_strlen
#define strncpy eteros_strncpy
#define strlcpy eteros_strlcpy
#define strcmp eteros_strcmp
#define itoa_s eteros_itoa_s
#define utoa_hex_s eteros_utoa_hex_s

#include "../kernel/string.c"

void hal_console_write(const char* s) {
    (void)s;
}

#include "../kernel/stdio.c"

#undef clock
#undef CLOCKS_PER_SEC
extern clock_t clock (void) __THROW;
#define CLOCKS_PER_SEC 1000000l

int main() {
    char buf1[1024];
    clock_t start, end;
    double time_taken;

    start = clock();
    for (int i = 0; i < 1000000; i++) {
        snprintf(buf1, sizeof(buf1), "Test %d %x %s %p", 123456789, 0xDEADBEEF, "Hello World", (void*)0x12345678);
    }
    end = clock();
    time_taken = (double)(end - start) / CLOCKS_PER_SEC;
    printf("snprintf time: %f s\n", time_taken);

    return 0;
}
