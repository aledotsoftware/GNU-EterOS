#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

/* Define host test mode */
#define __ETEROS_HOST_TEST__
#include "../include/string.h"

int main() {
    printf("Running string tests...\n");

    /* Test itoa_s */
    {
        char buffer[32];
        itoa_s(12345, buffer, sizeof(buffer), 10);
        assert(strcmp(buffer, "12345") == 0);

        itoa_s(-12345, buffer, sizeof(buffer), 10);
        assert(strcmp(buffer, "-12345") == 0);

        itoa_s(0, buffer, sizeof(buffer), 10);
        assert(strcmp(buffer, "0") == 0);

        /* Test truncation */
        char small_buf[4]; /* Holds 3 chars + null */
        itoa_s(12345, small_buf, sizeof(small_buf), 10);
        /* Expect "123" because itoa_s writes most significant digits first */
        printf("Small buffer itoa: %s\n", small_buf);
        assert(strcmp(small_buf, "123") == 0);
    }

    /* Test utoa_hex_s */
    {
        char buffer[32];
        utoa_hex_s(0xDEADBEEF, buffer, sizeof(buffer));
        /* Should be "0x00000000DEADBEEF" */
        printf("Full buffer utoa_hex: %s\n", buffer);
        assert(strcmp(buffer, "0x00000000DEADBEEF") == 0);

        /* Test truncation */
        char small_buf[5]; /* Holds 4 chars + null */
        utoa_hex_s(0xDEADBEEF, small_buf, sizeof(small_buf));
        /* Should contain "0x00" */
        printf("Small buffer utoa_hex: %s\n", small_buf);
        assert(strcmp(small_buf, "0x00") == 0);
    }

    /* Test memcpy */
    {
        char src[] = "Hello World";
        char dest[20];

        /* Test normal copy */
        memset(dest, 0, sizeof(dest));
        memcpy(dest, src, strlen(src) + 1);
        assert(strcmp(dest, "Hello World") == 0);

        /* Test small copy (less than 8 bytes) */
        memset(dest, 0, sizeof(dest));
        memcpy(dest, src, 5);
        dest[5] = '\0';
        assert(strcmp(dest, "Hello") == 0);

        /* Test 8 bytes exactly */
        memset(dest, 0, sizeof(dest));
        memcpy(dest, src, 8);
        dest[8] = '\0';
        assert(strcmp(dest, "Hello Wo") == 0);

        /* Test 9 bytes */
        memset(dest, 0, sizeof(dest));
        memcpy(dest, src, 9);
        dest[9] = '\0';
        assert(strcmp(dest, "Hello Wor") == 0);

        /* Test aligned/unaligned (if we could control alignment, but here just generic) */
        /* Test 0 bytes */
        memset(dest, 'x', sizeof(dest));
        memcpy(dest, src, 0);
        assert(dest[0] == 'x');
    }

    printf("All tests passed!\n");
    return 0;
}
