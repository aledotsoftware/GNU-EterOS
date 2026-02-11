#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>

/* Capture standard library functions before they are renamed */
static void* (*std_memset)(void*, int, size_t) = memset;

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

    /* Test itoa_s with edge cases */
    {
        char buffer[65];

        /* INT64_MIN */
        itoa_s(INT64_MIN, buffer, sizeof(buffer), 10);
        printf("INT64_MIN: %s\n", buffer);
        /* Check against expected string */
        /* INT64_MIN = -9223372036854775808 */
        assert(strcmp(buffer, "-9223372036854775808") == 0);

        /* INT64_MAX */
        itoa_s(INT64_MAX, buffer, sizeof(buffer), 10);
        printf("INT64_MAX: %s\n", buffer);
        assert(strcmp(buffer, "9223372036854775807") == 0);

        /* Base 16 (INT64_MIN) - should be treated as unsigned */
        itoa_s(INT64_MIN, buffer, sizeof(buffer), 16);
        printf("INT64_MIN in base 16: %s\n", buffer);
        assert(strcmp(buffer, "8000000000000000") == 0);

        /* Base 16 (negative number) - should be treated as unsigned */
        itoa_s(-1, buffer, sizeof(buffer), 16);
        printf("-1 in base 16: %s\n", buffer);
        assert(strcmp(buffer, "FFFFFFFFFFFFFFFF") == 0);

        /* Base 2 */
        itoa_s(5, buffer, sizeof(buffer), 2);
        printf("5 in base 2: %s\n", buffer);
        assert(strcmp(buffer, "101") == 0);

        /* Base 2 (INT64_MIN) */
        itoa_s(INT64_MIN, buffer, sizeof(buffer), 2);
        printf("INT64_MIN in base 2: %s\n", buffer);
        /* 1 followed by 63 zeros */
        assert(buffer[0] == '1');
        for (int i = 1; i < 64; i++) {
            assert(buffer[i] == '0');
        }
        assert(buffer[64] == '\0');

        /* Truncation with negative number */
        char small_buf[5]; /* Size 5 -> 4 chars + null */
        itoa_s(-12345, small_buf, sizeof(small_buf), 10);
        printf("Small buffer negative: %s\n", small_buf);
        /* -12345 (6 chars) -> truncation to 4 chars -> "-123" */
        assert(strcmp(small_buf, "-123") == 0);

        printf("Advanced itoa_s tests passed\n");
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

    /* Test utoa_hex */
    {
        char buffer[32];
        utoa_hex(0xDEADBEEF, buffer);
        /* Should be "0x00000000DEADBEEF" (18 chars) */
        printf("utoa_hex: %s\n", buffer);
        assert(strcmp(buffer, "0x00000000DEADBEEF") == 0);

        utoa_hex(0, buffer);
        assert(strcmp(buffer, "0x0000000000000000") == 0);

        utoa_hex(0xFFFFFFFFFFFFFFFF, buffer);
        assert(strcmp(buffer, "0xFFFFFFFFFFFFFFFF") == 0);

        printf("utoa_hex tests passed\n");
    }

    /* Test memset */
    {
        uint8_t buffer[32];
        void* ret;

        /* Initialize with garbage or known pattern using standard memset */
        std_memset(buffer, 0x55, sizeof(buffer));

        /* Test 1: Fill with 0xAA */
        ret = memset(buffer, 0xAA, 32);
        assert(ret == buffer);
        for(int i=0; i<32; i++) {
            assert(buffer[i] == 0xAA);
        }

        /* Test 2: Fill partial */
        std_memset(buffer, 0x55, sizeof(buffer));
        memset(buffer, 0xBB, 16);
        for(int i=0; i<16; i++) assert(buffer[i] == 0xBB);
        for(int i=16; i<32; i++) assert(buffer[i] == 0x55); /* Remaining should be unchanged */

        /* Test 3: Size 0 */
        buffer[0] = 0xCC;
        memset(buffer, 0xDD, 0);
        assert(buffer[0] == 0xCC);

        /* Test 4: Value truncation */
        memset(buffer, 0x1234, 32);
        for(int i=0; i<32; i++) assert(buffer[i] == 0x34);

        /* Test 5: Unaligned size */
        std_memset(buffer, 0x55, sizeof(buffer));
        memset(buffer, 0xEE, 7);
        for(int i=0; i<7; i++) assert(buffer[i] == 0xEE);
        assert(buffer[7] == 0x55);

        printf("memset tests passed\n");
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

    /* Test memcpy */
    {
        char src[] = "Hello World";
        char dest[20];

        /* Test normal copy */
        std_memset(dest, 0, sizeof(dest));
        memcpy(dest, src, strlen(src) + 1);
        assert(strcmp(dest, "Hello World") == 0);

        /* Test small copy (less than 8 bytes) */
        std_memset(dest, 0, sizeof(dest));
        memcpy(dest, src, 5);
        dest[5] = '\0';
        assert(strcmp(dest, "Hello") == 0);

        /* Test 8 bytes exactly */
        std_memset(dest, 0, sizeof(dest));
        memcpy(dest, src, 8);
        dest[8] = '\0';
        assert(strcmp(dest, "Hello Wo") == 0);

        /* Test 9 bytes */
        std_memset(dest, 0, sizeof(dest));
        memcpy(dest, src, 9);
        dest[9] = '\0';
        assert(strcmp(dest, "Hello Wor") == 0);

        /* Test aligned/unaligned (if we could control alignment, but here just generic) */
        /* Test 0 bytes */
        std_memset(dest, 'x', sizeof(dest));
        memcpy(dest, src, 0);
        assert(dest[0] == 'x');
    }

    /* Test strlen */
    {
        /* Test normal string */
        assert(strlen("Hello World") == 11);

        /* Test empty string */
        assert(strlen("") == 0);

        /* Test single character */
        assert(strlen("A") == 1);

        /* Test string with spaces */
        assert(strlen("   ") == 3);

        printf("strlen tests passed\n");
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

    /* Test strlcpy null termination guarantee */
    {
        char buffer[10];
        size_t len;

        /* Initialize with pattern */
        memset(buffer, 'A', sizeof(buffer));

        /* Truncated copy */
        len = strlcpy(buffer, "1234567890", 5);
        assert(len == 10);
        assert(buffer[0] == '1');
        assert(buffer[1] == '2');
        assert(buffer[2] == '3');
        assert(buffer[3] == '4');
        assert(buffer[4] == '\0'); /* Null terminator */
        assert(buffer[5] == 'A'); /* Should be untouched */

        printf("strlcpy null termination guarantee passed\n");
    }

    /* Test memset16 */
    {
        uint16_t buffer[16];
        /* Fill with pattern 0xAABB */
        memset16(buffer, 0xAABB, 16);
        for(int i=0; i<16; i++) {
            assert(buffer[i] == 0xAABB);
        }
        printf("memset16 test passed\n");
    }

    /* Test memmove (overlap handling) */
    {
        char buffer[20];

        /* Initialize buffer: "0123456789" */
        strcpy(buffer, "0123456789");

        /* Overlap: dest > src (shift right, requires backward copy) */
        /* src = buffer ("01234..."), dest = buffer + 1 ("12345...") */
        /* Move "01234" to index 1 -> overwrites "12345" */
        /* If forward copy: buf[1]='0', buf[2]='0' (was '1' but now '0')... WRONG */
        /* If backward copy: buf[5]='4', buf[4]='3', ... buf[1]='0'. CORRECT */
        memmove(buffer + 1, buffer, 5);
        /* Expected: "0012346789" */
        /* buffer[0] is untouched ('0') */
        /* buffer[1..5] becomes "01234" */
        /* buffer[6..9] remains "6789" */
        assert(memcmp(buffer, "0012346789", 10) == 0);

        printf("memmove backward copy (dest > src) passed\n");

        /* Reset */
        strcpy(buffer, "0123456789");

        /* Overlap: dest < src (shift left, forward copy is safe) */
        /* src = buffer + 1 ("12345..."), dest = buffer ("01234...") */
        /* Move "12345" to index 0 -> overwrites "01234" */
        memmove(buffer, buffer + 1, 5);
        /* Expected: "1234556789" */
        /* buffer[0..4] becomes "12345" */
        /* buffer[5..9] remains "56789" */
        assert(memcmp(buffer, "1234556789", 10) == 0);

        printf("memmove forward copy (dest < src) passed\n");

        /* No overlap */
        strcpy(buffer, "0123456789");
        memmove(buffer + 5, buffer, 5);
        /* Expected: "0123401234" */
        assert(memcmp(buffer, "0123401234", 10) == 0);

        printf("memmove no overlap passed\n");

        /* Self assignment */
        strcpy(buffer, "0123456789");
        memmove(buffer, buffer, 10);
        assert(memcmp(buffer, "0123456789", 10) == 0);

        printf("memmove self assignment passed\n");
    }

    printf("All tests passed!\n");
    return 0;
}
