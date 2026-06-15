// #define __ETEROS_HOST_TEST__

/* Use absolute paths to avoid shadowing by -Iuserspace/libc/include */
#include "/usr/include/stdio.h"
#include "/usr/include/string.h"
#include "/usr/include/time.h"
#include "/usr/include/stdint.h"
#include "/usr/include/stdlib.h"

/* Save standard function pointers before they get renamed */
size_t (*std_strlen)(const char *) = strlen;
void *(*std_malloc)(size_t) = malloc;
void (*std_free)(void *) = free;

#include "../userspace/libc/include/string.h"

/* Include the implementation directly to compile it with renames */
#include "../userspace/libc/src/string.c"

/* Wrapper for malloc/free since they are renamed but not implemented in string.c */
void *eteros_malloc(size_t size) { return std_malloc(size); }
void eteros_free(void *ptr) { std_free(ptr); }
/* We don't use calloc/realloc/abort in this test, but if referenced they need wrappers */
void eteros_abort(void) { abort(); }

void verify_strlen() {
    printf("Verifying eteros_strlen...\n");
    const char *cases[] = {
        "",
        "a",
        "ab",
        "abc",
        "abcd",
        "abcde",
        "abcdef",
        "abcdefg",
        "abcdefgh",
        "abcdefghi",
        "01234567890123456789",
        "The quick brown fox jumps over the lazy dog",
        NULL
    };

    for (int i = 0; cases[i]; i++) {
        size_t std_len = std_strlen(cases[i]);
        size_t et_len = eteros_strlen(cases[i]);
        if (std_len != et_len) {
            printf("FAIL: eteros_strlen(\"%s\") = %zu, expected %zu\n", cases[i], et_len, std_len);
            exit(1);
        }
    }

    /* Unaligned test */
    char buffer[32];
    /* Using eteros_strcpy since standard strcpy is renamed */
    eteros_strlcpy(buffer, "1234567890", sizeof(buffer));

    for (int i = 0; i < 8; i++) {
        size_t std_len = std_strlen(buffer + i);
        size_t et_len = eteros_strlen(buffer + i);
        if (std_len != et_len) {
            printf("FAIL: Unaligned (+%d) eteros_strlen = %zu, expected %zu\n", i, et_len, std_len);
            exit(1);
        }
    }
    printf("Verification passed.\n");
}

void benchmark_strlen() {
    printf("Benchmarking eteros_strlen vs standard strlen...\n");
    size_t size = 10 * 1024 * 1024; /* 10MB */
    char *buffer = malloc(size + 1);
    if (!buffer) {
        printf("Malloc failed\n");
        return;
    }

    /* Using eteros_memset */
    eteros_memset(buffer, 'A', size);
    buffer[size] = '\0';

    int iterations = 100;
    clock_t start, end;
    double cpu_time_used;

    /* Standard strlen */
    start = clock();
    volatile size_t total_len_std = 0;
    for (int i = 0; i < iterations; i++) {
        total_len_std += std_strlen(buffer);
    }
    end = clock();
    cpu_time_used = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Standard strlen: %f seconds (Total len: %zu)\n", cpu_time_used, total_len_std);

    /* Eteros strlen */
    start = clock();
    volatile size_t total_len_et = 0;
    for (int i = 0; i < iterations; i++) {
        total_len_et += eteros_strlen(buffer);
    }
    end = clock();
    double et_time = ((double) (end - start)) / CLOCKS_PER_SEC;
    printf("Eteros strlen:   %f seconds (Total len: %zu)\n", et_time, total_len_et);

    if (et_time > 0) {
        printf("Speedup: %.2fx\n", cpu_time_used / et_time);
    }

    free(buffer);
}

int main() {
    verify_strlen();
    benchmark_strlen();
    return 0;
}
