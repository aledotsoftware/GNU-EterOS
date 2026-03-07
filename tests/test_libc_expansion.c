#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>
#include <ctype.h>

/*
 * We included userspace headers above (via -I).
 * They defined macros renaming functions to eteros_xxx.
 * e.g. malloc -> eteros_malloc, printf -> eteros_printf.
 *
 * We want to implement wrappers for malloc/exit using HOST functions.
 * So we must undef macros, implement wrapper calling host function, then redefine macro.
 */

#undef malloc
#undef free
#undef calloc
#undef realloc
#undef exit

// Wrappers
void *eteros_malloc(size_t size) {
    return malloc(size); // Calls host malloc
}

void eteros_free(void *ptr) {
    free(ptr);
}

void *eteros_calloc(size_t nmemb, size_t size) {
    return calloc(nmemb, size);
}

void *eteros_realloc(void *ptr, size_t size) {
    return realloc(ptr, size);
}

void eteros_exit(int status) {
    exit(status);
}

// Restore macros so source files use our wrappers
#define malloc eteros_malloc
#define free eteros_free
#define calloc eteros_calloc
#define realloc eteros_realloc
#define exit eteros_exit

/*
 * For other functions (printf, strtol, etc.), we keep the macros active.
 * The included source files will implement eteros_xxx.
 * Our test code using xxx(...) will call eteros_xxx.
 */

#define TEST_ASSERT(cond) do { \
    if (!(cond)) { \
        printf("FAILED: %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        eteros_exit(1); \
    } \
} while(0)

// Include source files
#include "../userspace/libc/src/ctype.c"
#include "../userspace/libc/src/string.c"
#include "../userspace/libc/src/stdlib.c"
#include "../userspace/libc/src/stdio.c"
#include "../userspace/libc/src/time.c"
#include "../userspace/libc/src/assert.c"

void test_ctype() {
    printf("Testing ctype...\n");
    TEST_ASSERT(isalnum('a'));
    TEST_ASSERT(isalnum('1'));
    TEST_ASSERT(!isalnum('@'));

    TEST_ASSERT(isalpha('A'));
    TEST_ASSERT(!isalpha('1'));

    TEST_ASSERT(isdigit('0'));
    TEST_ASSERT(!isdigit('a'));

    TEST_ASSERT(tolower('A') == 'a');
    TEST_ASSERT(toupper('z') == 'Z');

    TEST_ASSERT(isspace(' '));
    TEST_ASSERT(isspace('\n'));
    TEST_ASSERT(!isspace('A'));
    printf("ctype tests passed.\n");
}

void test_string() {
    printf("Testing string...\n");

    printf("Testing strdup...\n");
    char *dup = strdup("Hello");
    TEST_ASSERT(dup != NULL);
    TEST_ASSERT(strcmp(dup, "Hello") == 0);
    free(dup);

    printf("Testing strndup...\n");
    dup = strndup("Hello World", 5);
    TEST_ASSERT(dup != NULL);
    TEST_ASSERT(strcmp(dup, "Hello") == 0);
    free(dup);

    printf("Testing strspn...\n");
    TEST_ASSERT(strspn("hello", "he") == 2);
    TEST_ASSERT(strspn("hello", "x") == 0);

    printf("Testing strcspn...\n");
    TEST_ASSERT(strcspn("hello", "l") == 2);
    TEST_ASSERT(strcspn("hello", "xyz") == 5);

    printf("Testing strpbrk...\n");
    TEST_ASSERT(strpbrk("hello", "l") != NULL);
    TEST_ASSERT(*strpbrk("hello", "l") == 'l');
    TEST_ASSERT(strpbrk("hello", "xyz") == NULL);

    printf("Testing strerror...\n");
    const char *err = strerror(EPERM);
    TEST_ASSERT(err != NULL);
    printf("strerror(EPERM) = %s\n", err);
    TEST_ASSERT(strstr(err, "Operation not permitted") != NULL);

    printf("string tests passed.\n");
}

int compare_int(const void *a, const void *b) {
    return (*(int*)a - *(int*)b);
}

void test_stdlib() {
    printf("Testing stdlib...\n");

    // strtol
    char *end;
    long l = strtol("123", &end, 10);
    TEST_ASSERT(l == 123);
    TEST_ASSERT(*end == '\0');

    l = strtol("  -123xyz", &end, 10);
    TEST_ASSERT(l == -123);
    TEST_ASSERT(*end == 'x');

    l = strtol("0xFF", &end, 16);
    TEST_ASSERT(l == 255);

    // abs
    TEST_ASSERT(abs(-10) == 10);

    // qsort
    int arr[] = {3, 1, 4, 1, 5};
    qsort(arr, 5, sizeof(int), compare_int);
    TEST_ASSERT(arr[0] == 1);
    TEST_ASSERT(arr[1] == 1);
    TEST_ASSERT(arr[2] == 3);
    TEST_ASSERT(arr[3] == 4);
    TEST_ASSERT(arr[4] == 5);

    printf("stdlib tests passed.\n");
}

void test_stdio() {
    printf("Testing stdio...\n");
    char buf[100];
    int ret;

    // snprintf
    ret = snprintf(buf, sizeof(buf), "Hello %s %d", "World", 123);
    TEST_ASSERT(ret == 15);
    TEST_ASSERT(strcmp(buf, "Hello World 123") == 0);

    // snprintf
    ret = snprintf(buf, 5, "Hello");
    TEST_ASSERT(ret == 5);
    TEST_ASSERT(strcmp(buf, "Hell") == 0);

    printf("stdio tests passed.\n");
}

void test_time() {
    printf("Testing time...\n");
    struct tm tm;
    memset(&tm, 0, sizeof(tm));
    tm.tm_year = 2023 - 1900;
    tm.tm_mon = 0; // Jan
    tm.tm_mday = 1;

    time_t t = mktime(&tm);

    struct tm *tm2 = gmtime(&t);
    TEST_ASSERT(tm2->tm_year == tm.tm_year);
    TEST_ASSERT(tm2->tm_mon == tm.tm_mon);
    TEST_ASSERT(tm2->tm_mday == tm.tm_mday);

    char *s = ctime(&t);
    printf("ctime: %s", s);
    TEST_ASSERT(strstr(s, "2023") != NULL);
    TEST_ASSERT(strstr(s, "Jan") != NULL);

    printf("time tests passed.\n");
}

void test_assert_check() {
    printf("Testing assert...\n");
    assert(1);
    printf("assert tests passed.\n");
}

int main() {
    printf("Running libc expansion tests...\n");

    test_ctype();
    test_string();
    test_stdlib();
    test_stdio();
    test_time();
    test_assert_check();

    printf("All tests passed!\n");
    return 0;
}
