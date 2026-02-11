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

    /* Test strlcpy */
    {
        char buffer[32];
        size_t len;

        /* Normal copy */
        len = strlcpy(buffer, "Hello", sizeof(buffer));
        assert(len == 5);
        assert(strcmp(buffer, "Hello") == 0);

        /* Truncation */
        len = strlcpy(buffer, "Hello World", 6);
        assert(len == 11); /* Returns src length */
        assert(strcmp(buffer, "Hello") == 0); /* buffer holds "Hello\0" */

        /* Empty source */
        len = strlcpy(buffer, "", sizeof(buffer));
        assert(len == 0);
        assert(strcmp(buffer, "") == 0);

        /* Size 0 */
        buffer[0] = 'X';
        len = strlcpy(buffer, "Test", 0);
        assert(len == 4);
        assert(buffer[0] == 'X'); /* Should not modify buffer */

        /* Size 1 */
        buffer[0] = 'X';
        buffer[1] = 'Y';
        len = strlcpy(buffer, "Test", 1);
        assert(len == 4);
        assert(buffer[0] == '\0'); /* Should be empty string */
        assert(buffer[1] == 'Y'); /* Should not touch beyond size */

        printf("strlcpy tests passed\n");
    }

    printf("All tests passed!\n");
    return 0;
}
