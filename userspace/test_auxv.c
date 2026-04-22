#include <stdio.h>
#include <stdint.h>

extern uint64_t __auxv_pagesz;
extern uint64_t __auxv_phdr;
extern uint64_t __auxv_phent;
extern uint64_t __auxv_phnum;
extern uint64_t __auxv_entry;
extern uint64_t __auxv_base;
extern uint64_t __auxv_random;

int main(void) {
    int fail = 0;

    printf("=== auxv smoke test ===\n");
    printf("AT_PAGESZ=%llu\n", (unsigned long long)__auxv_pagesz);
    printf("AT_PHDR=%llu\n", (unsigned long long)__auxv_phdr);
    printf("AT_PHENT=%llu\n", (unsigned long long)__auxv_phent);
    printf("AT_PHNUM=%llu\n", (unsigned long long)__auxv_phnum);
    printf("AT_ENTRY=0x%llx\n", (unsigned long long)__auxv_entry);
    printf("AT_BASE=0x%llx\n", (unsigned long long)__auxv_base);
    printf("AT_RANDOM=0x%llx\n", (unsigned long long)__auxv_random);

    if (__auxv_pagesz != 4096ULL) {
        printf("FAIL: AT_PAGESZ expected 4096\n");
        fail = 1;
    }
    if (__auxv_entry == 0ULL) {
        printf("FAIL: AT_ENTRY should be non-zero\n");
        fail = 1;
    }
    if (__auxv_phent == 0ULL) {
        printf("FAIL: AT_PHENT should be non-zero\n");
        fail = 1;
    }
    if (__auxv_phnum == 0ULL) {
        printf("FAIL: AT_PHNUM should be non-zero\n");
        fail = 1;
    }

    if (fail) {
        printf("RESULT: FAIL\n");
        return 1;
    }

    printf("RESULT: PASS\n");
    return 0;
}
