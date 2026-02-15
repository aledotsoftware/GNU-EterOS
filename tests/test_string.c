#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

/* Capture standard library functions before they are renamed */
static void* (*std_memset)(void*, int, size_t) = memset;
static int (*std_memcmp)(const void*, const void*, size_t) = memcmp;
static int (*std_strcmp)(const char*, const char*) = strcmp;

/* Define host test mode */
#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif
#include "../include/string.h"

void benchmark_memcmp() {
    const size_t size = 1024 * 1024; /* 1 MB */
    const int iterations = 1000;

    char* buf1 = malloc(size);
    char* buf2 = malloc(size);

    if (!buf1 || !buf2) {
        printf("Benchmark failed: malloc error\n");
        return;
    }

    /* Fill buffers (make them identical to force full comparison) */
    std_memset(buf1, 0xAA, size);
    std_memset(buf2, 0xAA, size);

    clock_t start = clock();

    volatile int res = 0; /* Prevent optimization */
    for (int i = 0; i < iterations; i++) {
        res += memcmp(buf1, buf2, size);
    }

    clock_t end = clock();
    double time_taken = ((double)(end - start)) / CLOCKS_PER_SEC;

    printf("Benchmark memcmp: %d iterations of 1MB comparison took %f seconds\n", iterations, time_taken);

    free(buf1);
    free(buf2);
}

int main() {
    benchmark_memcmp();
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

    /* Test memcmp */
    {
        char b1[20];
        char b2[20];

        /* Test 1: Equality */
        std_memset(b1, 0xAB, 20);
        std_memset(b2, 0xAB, 20);
        assert(memcmp(b1, b2, 20) == 0);
        assert(memcmp(b1, b2, 0) == 0); /* Size 0 */

        /* Test 2: Inequality */
        b2[10] = 0xAC; /* b2 > b1 */
        /* b1[10] = 0xAB, b2[10] = 0xAC */
        /* memcmp returns b1 - b2 -> negative */
        assert(memcmp(b1, b2, 20) < 0);

        b2[10] = 0xAA; /* b2 < b1 */
        /* b1[10] = 0xAB, b2[10] = 0xAA */
        /* memcmp returns b1 - b2 -> positive */
        assert(memcmp(b1, b2, 20) > 0);

        /* Test 3: Difference at start */
        b1[0] = 'A';
        b2[0] = 'B';
        assert(memcmp(b1, b2, 20) < 0);

        /* Test 4: Difference at end */
        std_memset(b1, 0, 20);
        std_memset(b2, 0, 20);
        b1[19] = 1;
        assert(memcmp(b1, b2, 20) > 0);

        /* Test 5: Verify against standard memcmp */
        /* Randomize buffers */
        for(int i=0; i<20; i++) {
            b1[i] = (char)(i * 3);
            b2[i] = (char)(i * 3);
        }
        b2[10] = 50; /* Introduce difference */

        int res_std = std_memcmp(b1, b2, 20);
        int res_eteros = memcmp(b1, b2, 20);

        /* Check sign consistency */
        if (res_std < 0) assert(res_eteros < 0);
        else if (res_std > 0) assert(res_eteros > 0);
        else assert(res_eteros == 0);

        /* Test 6: Unsigned comparison */
        /* char is usually signed, but memcmp treats as unsigned char */
        unsigned char u1[] = { 0xFF };
        unsigned char u2[] = { 0x00 };
        /* 0xFF (255) > 0x00 (0) */
        /* If treated as signed char: -1 < 0 -> WRONG */
        assert(memcmp(u1, u2, 1) > 0);

        printf("memcmp tests passed\n");
    }

    /* Test strcmp */
    {
        /* Test 1: Equality */
        assert(strcmp("Hello", "Hello") == 0);
        assert(strcmp("", "") == 0);

        /* Test 2: Inequality */
        assert(strcmp("Hello", "World") < 0); /* 'H' < 'W' */
        assert(strcmp("World", "Hello") > 0);

        /* Test 3: Prefix */
        assert(strcmp("Hello", "Hello World") < 0); /* '\0' < ' ' */
        assert(strcmp("Hello World", "Hello") > 0);

        /* Test 4: Empty strings */
        assert(strcmp("", "A") < 0);
        assert(strcmp("A", "") > 0);

        /* Test 5: Case sensitivity */
        assert(strcmp("A", "a") < 0); /* 65 < 97 */

        /* Test 6: Verify against standard strcmp */
        const char* s1 = "TestString1";
        const char* s2 = "TestString2";
        int res_std = std_strcmp(s1, s2);
        int res_eteros = strcmp(s1, s2);

        if (res_std < 0) assert(res_eteros < 0);
        else if (res_std > 0) assert(res_eteros > 0);
        else assert(res_eteros == 0);

        /* Test 7: Extended ASCII / Signedness */
        /* strcmp should treat chars as unsigned char */
        char u1[] = { (char)0xFF, '\0' }; /* 255 */
        char u2[] = { 0x01, '\0' };       /* 1 */

        /* If char is signed, 0xFF is -1. -1 < 1. */
        /* If unsigned comparison, 255 > 1. */
        /* Standard strcmp uses unsigned char comparison. */
        assert(strcmp(u1, u2) > 0);

        printf("strcmp tests passed\n");
    }

    printf("All tests passed!\n");
    return 0;
}
