#define __ETEROS_HOST_TEST__
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>

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

int main() {
    char buf[64];
    clock_t start = clock();
    for (int i = 0; i < 10000000; i++) {
        eteros_itoa_s(123456789, buf, sizeof(buf), 10);
        eteros_itoa_s(-987654321, buf, sizeof(buf), 10);
        eteros_itoa_s(0xDEADBEEF, buf, sizeof(buf), 16);
    }
    clock_t end = clock();
    double time_taken = (double)(end - start) / CLOCKS_PER_SEC;
    printf("itoa_s time: %f s\n", time_taken);
    return 0;
}
