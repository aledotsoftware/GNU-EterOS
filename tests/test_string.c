#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>

/* Define host test mode */
#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif
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

        /* Test boundary conditions */
        char temp[20];

        /* Size 1 */
        temp[0] = 'a';
        utoa_hex_s(0xDEADBEEF, temp, 1);
        assert(temp[0] == '\0');

        /* Size 2 */
        utoa_hex_s(0xDEADBEEF, temp, 2);
        assert(strcmp(temp, "0") == 0);

        /* Size 3 */
        utoa_hex_s(0xDEADBEEF, temp, 3);
        assert(strcmp(temp, "0x") == 0);

        /* Size 18 (0x + 15 digits + null) - truncates last digit */
        char buf18[18];
        utoa_hex_s(0xDEADBEEF, buf18, 18);
        assert(strlen(buf18) == 17);
        assert(strncmp(buf18, "0x00000000DEADBEE", 17) == 0);

        /* Size 19 (full) */
        char buf19[19];
        utoa_hex_s(0xDEADBEEF, buf19, 19);
        assert(strlen(buf19) == 18);
        assert(strcmp(buf19, "0x00000000DEADBEEF") == 0);

        printf("Boundary tests passed for utoa_hex_s\n");
    }

    /* Test memcpy */
    {
        char src[] = "Hello World";
        char dest[32];
        void* ret;

        /* Test basic copy */
        /* Use standard memset to clear buffer initially to be safe,
           or just loop to clear it manually if we don't trust memset yet */
        for(size_t i=0; i<sizeof(dest); i++) dest[i] = 0;

        ret = memcpy(dest, src, strlen(src) + 1);
        assert(ret == dest);
        assert(strcmp(dest, src) == 0);

        /* Test partial copy */
        for(size_t i=0; i<sizeof(dest); i++) dest[i] = 0;
        ret = memcpy(dest, src, 5);
        assert(ret == dest);
        /* Check first 5 chars match */
        for(int i=0; i<5; i++) {
            assert(dest[i] == src[i]);
        }
        /* Check 6th char is still 0 */
        assert(dest[5] == 0);

        /* Test zero length copy */
        for(size_t i=0; i<sizeof(dest); i++) dest[i] = 'X';
        ret = memcpy(dest, src, 0);
        assert(ret == dest);
        assert(dest[0] == 'X');

        printf("memcpy tests passed\n");
    }

    printf("All tests passed!\n");
    return 0;
}
