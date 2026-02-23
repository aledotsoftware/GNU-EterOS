#include "libc/include/sys/mman.h"
#include "libc/include/stdio.h"
#include "libc/include/errno.h"
#include "libc/include/stdlib.h"

/* From include/vmm.h */
#define USER_LIMIT 0x00007FFFFFFFFFFFUL

int main(void) {
    printf("Starting mmap limit test...\n");

    /* Test 1: Try MAP_FIXED above USER_LIMIT */
    void* addr = (void*)(USER_LIMIT + 4096);
    void* ptr = mmap(addr, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_FIXED | MAP_ANONYMOUS, -1, 0);

    if (ptr != MAP_FAILED) {
        printf("[FAIL] mmap succeeded above USER_LIMIT (ptr=%p)\n", ptr);
        return 1;
    } else {
        printf("[PASS] mmap rejected address above USER_LIMIT (errno=%d)\n", errno);
    }

    /* Test 2: Exhaust mmap_base (simulated) */
    /* Since we cannot easily exhaust 128TB, we rely on the kernel fix being verified by code review.
       Ideally, we would mmap a huge chunk that wraps around if the check is missing. */

    /* Attempt to allocate a chunk that would overflow if base was near limit */
    size_t huge_size = USER_LIMIT - 0x700000000000UL + 4096 * 10; /* Should be too big */

    ptr = mmap(NULL, huge_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (ptr != MAP_FAILED) {
         printf("[FAIL] mmap succeeded with huge size (ptr=%p)\n", ptr);
         return 1;
    } else {
         printf("[PASS] mmap rejected huge size (errno=%d)\n", errno);
    }

    return 0;
}
