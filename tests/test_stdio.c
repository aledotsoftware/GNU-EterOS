#include <stdio.h> /* This includes ../include/stdio.h due to -Iinclude, which pulls system stdio via include_next */
#include <string.h> /* This includes ../include/string.h due to -Iinclude */
#include <hal.h> /* For hal_console_write prototype */

/* We need to define asserts manually or include <assert.h> */
/* <assert.h> is system header, likely not in include/, so it picks system one. */
#include <assert.h>
#include <stdint.h> /* For INT64_MIN */

/* Mock hal_console_write */
static char mock_console_buffer[4096];

void hal_console_write(const char* s) {
    /* Use eteros_strlen/strcat or just standard logic? */
    /* Since string.h is included, strlen -> eteros_strlen */
    size_t len = strlen(mock_console_buffer);
    size_t add_len = strlen(s);

    if (len + add_len < sizeof(mock_console_buffer)) {
        /* We can implement strcat or just simple copy */
        char* dest = mock_console_buffer + len;
        while (*s) {
            *dest++ = *s++;
        }
        *dest = '\0';
    }
}

void test_snprintf() {
    char buf[128];
    int ret;

    /* Basic integer */
    ret = snprintf(buf, sizeof(buf), "%d", 123);
    ASSERT(strcmp(buf, "123") == 0);
    ASSERT(ret == 3);

    /* Negative integer */
    ret = snprintf(buf, sizeof(buf), "%d", -123);
    ASSERT(strcmp(buf, "-123") == 0);
    ASSERT(ret == 4);

    /* Hex (unsigned) */
    ret = snprintf(buf, sizeof(buf), "%x", 0xABC);
    ASSERT(strcmp(buf, "abc") == 0); /* Lowercase default */
    ASSERT(ret == 3);

    /* Hex (uppercase) */
    ret = snprintf(buf, sizeof(buf), "%X", 0xABC);
    ASSERT(strcmp(buf, "ABC") == 0);
    ASSERT(ret == 3);

    /* String */
    ret = snprintf(buf, sizeof(buf), "%s", "Hello");
    ASSERT(strcmp(buf, "Hello") == 0);
    ASSERT(ret == 5);

    /* Char */
    ret = snprintf(buf, sizeof(buf), "%c", 'A');
    ASSERT(strcmp(buf, "A") == 0);
    ASSERT(ret == 1);

    /* Pointer */
    void* ptr = (void*)(uintptr_t)0x1234;
    ret = snprintf(buf, sizeof(buf), "%p", ptr);
    /* Should be 0x1234 */
    ASSERT(strcmp(buf, "0x1234") == 0);

    /* Percent */
    ret = snprintf(buf, sizeof(buf), "%%");
    ASSERT(strcmp(buf, "%") == 0);
    ASSERT(ret == 1);

    /* Mixed */
    ret = snprintf(buf, sizeof(buf), "Val: %d, Hex: %x", 10, 255);
    ASSERT(strcmp(buf, "Val: 10, Hex: ff") == 0);

    /* Truncation */
    char small[5]; /* 4 + null */
    ret = snprintf(small, sizeof(small), "%s", "Hello World");
    ASSERT(ret == 11); /* Should return length needed */
    ASSERT(strcmp(small, "Hell") == 0); /* Should be truncated to fit */

    /* Zero padding */
    ret = snprintf(buf, sizeof(buf), "%05d", 123);
    ASSERT(strcmp(buf, "00123") == 0);

    /* Width padding (space) */
    ret = snprintf(buf, sizeof(buf), "%5d", 123);
    ASSERT(strcmp(buf, "  123") == 0);

    /* Left justification */
    ret = snprintf(buf, sizeof(buf), "%-5d", 123);
    ASSERT(strcmp(buf, "123  ") == 0);

    /* Left justification with zero flag (should ignore zero) */
    ret = snprintf(buf, sizeof(buf), "%-05d", 123);
    ASSERT(strcmp(buf, "123  ") == 0);

    /* String width padding */
    ret = snprintf(buf, sizeof(buf), "%10s", "Hello");
    ASSERT(strcmp(buf, "     Hello") == 0);

    /* String left justification */
    ret = snprintf(buf, sizeof(buf), "%-10s", "Hello");
    ASSERT(strcmp(buf, "Hello     ") == 0);

    /* String precision */
    ret = snprintf(buf, sizeof(buf), "%.3s", "Hello");
    ASSERT(strcmp(buf, "Hel") == 0);

    /* String width + precision */
    ret = snprintf(buf, sizeof(buf), "%10.3s", "Hello");
    ASSERT(strcmp(buf, "       Hel") == 0);

    /* String width + precision + left justification */
    ret = snprintf(buf, sizeof(buf), "%-10.3s", "Hello");
    ASSERT(strcmp(buf, "Hel       ") == 0);

    /* INT64_MIN */
    ret = snprintf(buf, sizeof(buf), "%ld", INT64_MIN);
    /* -9223372036854775808 */
    if (strcmp(buf, "-9223372036854775808") != 0) {
        printf("FAIL: INT64_MIN expected '-9223372036854775808', got '%s'\n", buf);
    }
    ASSERT(strcmp(buf, "-9223372036854775808") == 0);

    printf("snprintf tests passed\n");
}

void test_kprintf() {
    mock_console_buffer[0] = '\0';
    kprintf("Hello %s", "World");
    ASSERT(strcmp(mock_console_buffer, "Hello World") == 0);

    mock_console_buffer[0] = '\0';
    kprintf("Value: %d", 42);
    ASSERT(strcmp(mock_console_buffer, "Value: 42") == 0);

    printf("kprintf tests passed\n");
}

int main() {
    test_snprintf();
    test_kprintf();
    return 0;
}
void serial_write_string(const char* s) {}
