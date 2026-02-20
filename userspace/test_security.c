#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include <sys/mman.h>

/* Helper to print result */
void check(const char* test_name, int res, int expected_errno) {
    if (res < 0) {
        if (errno == expected_errno) {
            printf("[PASS] %s: Failed as expected with errno %d\n", test_name, errno);
        } else {
            printf("[FAIL] %s: Failed with errno %d, expected %d\n", test_name, errno, expected_errno);
        }
    } else {
        if (expected_errno == 0) {
            printf("[PASS] %s: Succeeded as expected\n", test_name);
        } else {
            printf("[FAIL] %s: Succeeded unexpectedly\n", test_name);
        }
    }
}

int main() {
    printf("Security Test: Syscall Pointer Validation\n");

    /* 1. Test Kernel Pointer (High Memory) */
    /* Current kernel is Higher Half at 0xFFFFFFFF80000000 or so */
    /* Let's try to write from a kernel address */
    char* kernel_ptr = (char*)0xFFFFFFFF80000000;
    int res = write(1, kernel_ptr, 1);
    check("Write from Kernel Pointer", res, EFAULT);

    /* 2. Test Unmapped User Pointer */
    /* 0x1000 is usually not mapped (NULL page protected?) */
    /* Or some random high user address */
    char* unmapped_ptr = (char*)0x0000700000000000;
    res = write(1, unmapped_ptr, 1);
    check("Write from Unmapped Pointer", res, EFAULT);

    /* 3. Test Valid User Pointer */
    char buf[] = "Hello";
    res = write(1, buf, 5);
    check("Write from Valid Pointer", res, 0);

    /* 4. Test Read-Only Pointer passed to Read (Buffer for writing) */
    /* We need a Read-Only page. String literals are usually RO .rodata */
    /* But standard write(fd, buf, count) reads from buf. So RO buf is OK for write(). */

    /* Let's test read(fd, buf, count). buf must be Writable. */
    /* If we pass a pointer to a string literal (RO), read should fail. */
    int pipes[2];
    if (pipe(pipes) != 0) {
        printf("Pipe failed\n");
        return 1;
    }

    char* ro_ptr = "ReadOnlyString";
    /* Try to read from pipe INTO ro_ptr. Should fail. */
    res = read(pipes[0], ro_ptr, 5);
    check("Read into Read-Only Pointer", res, EFAULT);

    /* 5. Test Write from Read-Only Pointer */
    /* write(fd, buf, count). buf is read-only. Should succeed. */
    res = write(pipes[1], ro_ptr, 5);
    check("Write from Read-Only Pointer", res, 0);

    return 0;
}
