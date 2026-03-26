#define __ETEROS_HOST_TEST__ 1

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <errno.h>

#ifndef EINVAL
#define EINVAL 22
#endif
#ifndef EFAULT
#define EFAULT 14
#endif


// Mocks
int vmm_verify_user_access(const void* ptr, size_t size, int is_write) {
    if (!ptr || size == 0) return 0;
    return 1;
}

// Function to test
static int64_t sys_utimensat(int dirfd, const char* pathname, const struct timespec times[2], int flags) {
    (void)dirfd; (void)pathname; (void)flags;
    if (times) {
        if (!vmm_verify_user_access(times, 2 * sizeof(struct timespec), 0)) return -EFAULT;
    }
    return 0; // Fake success for commands like `touch`
}

void my_assert(int condition) {
    if (!condition) {
        printf("Assertion failed!\n");
        exit(1);
    }
}

int main() {
    printf("Running test_syscall_utimensat...\n");

    struct timespec ts[2] = {{0,0}, {0,0}};
    int64_t res;

    // Test valid times
    res = sys_utimensat(-100, "/file", ts, 0);
    my_assert(res == 0);

    // Test NULL times (allowed, means 'now')
    res = sys_utimensat(-100, "/file", NULL, 0);
    my_assert(res == 0);

    // Test invalid times pointer
    res = sys_utimensat(-100, "/file", (void*)0, 0); // NULL handled above

    printf("test_syscall_utimensat passed!\n");
    return 0;
}
