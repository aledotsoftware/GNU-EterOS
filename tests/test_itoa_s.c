#define __ETEROS_HOST_TEST__
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

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
    eteros_itoa_s(123456789, buf, sizeof(buf), 10);
    printf("123456789 -> %s\n", buf);
    if(strcmp(buf, "123456789") != 0) return 1;

    eteros_itoa_s(-987654321, buf, sizeof(buf), 10);
    printf("-987654321 -> %s\n", buf);
    if(strcmp(buf, "-987654321") != 0) return 1;

    eteros_itoa_s(0, buf, sizeof(buf), 10);
    printf("0 -> %s\n", buf);
    if(strcmp(buf, "0") != 0) return 1;

    eteros_itoa_s(0xDEADBEEF, buf, sizeof(buf), 16);
    printf("0xDEADBEEF -> %s\n", buf);
    if(strcmp(buf, "DEADBEEF") != 0) return 1;

    eteros_itoa_s(-1, buf, sizeof(buf), 10);
    printf("-1 -> %s\n", buf);
    if(strcmp(buf, "-1") != 0) return 1;

    // INT64_MIN
    eteros_itoa_s(-9223372036854775807LL - 1, buf, sizeof(buf), 10);
    printf("INT64_MIN -> %s\n", buf);
    if(strcmp(buf, "-9223372036854775808") != 0) return 1;

    // INT64_MAX
    eteros_itoa_s(9223372036854775807LL, buf, sizeof(buf), 10);
    printf("INT64_MAX -> %s\n", buf);
    if(strcmp(buf, "9223372036854775807") != 0) return 1;

    printf("ALL TESTS PASSED\n");
    return 0;
}
