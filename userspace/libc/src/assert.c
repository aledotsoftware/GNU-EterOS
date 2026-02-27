#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

void eteros_assert_fail(const char *expr, const char *file, int line, const char *func) {
    dprintf(2, "Assertion failed: %s (%s: %s: %d)\n", expr, file, func, line);
    abort();
}
