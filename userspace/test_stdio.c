#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main() {
    printf("=== test_stdio ===\n");
    printf("Testing string: %s\n", "Hello");
    printf("Testing int: %d\n", 12345);
    printf("Testing hex: 0x%x\n", 0xABCD);

    char buf[100];
    snprintf(buf, sizeof(buf), "snprintf test: %d", 42);
    printf("%s\n", buf);

    return 0;
}
