#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

void test_malloc(void) {
    printf("[TEST] Testing malloc alignment...\n");
    void *p1 = malloc(1);
    void *p2 = malloc(10);
    void *p3 = malloc(100);

    printf("p1: %p\n", p1);
    printf("p2: %p\n", p2);
    printf("p3: %p\n", p3);

    if (((unsigned long)p1 & 0xF) != 0) printf("FAIL: p1 not 16-byte aligned\n");
    else printf("PASS: p1 aligned\n");

    if (((unsigned long)p2 & 0xF) != 0) printf("FAIL: p2 not 16-byte aligned\n");
    else printf("PASS: p2 aligned\n");

    if (((unsigned long)p3 & 0xF) != 0) printf("FAIL: p3 not 16-byte aligned\n");
    else printf("PASS: p3 aligned\n");

    printf("[TEST] Testing malloc/free/realloc...\n");
    char *s = malloc(100);
    if (!s) { printf("FAIL: malloc returned NULL\n"); return; }
    strcpy(s, "Hello World");
    if (strcmp(s, "Hello World") != 0) printf("FAIL: malloc write/read failed\n");
    else printf("PASS: malloc write/read\n");

    s = realloc(s, 200);
    if (!s) { printf("FAIL: realloc returned NULL\n"); return; }
    if (strcmp(s, "Hello World") != 0) printf("FAIL: realloc copy failed\n");
    else printf("PASS: realloc copy\n");

    strcat(s, " Extended");
    if (strcmp(s, "Hello World Extended") != 0) printf("FAIL: realloc extend failed\n");
    else printf("PASS: realloc extend\n");

    free(s);
    free(p1);
    free(p2);
    free(p3);

    /* Coalescing test */
    printf("[TEST] Testing coalescing...\n");
    void *a = malloc(100);
    void *b = malloc(100);
    void *c = malloc(100);
    printf("a=%p, b=%p, c=%p\n", a, b, c);

    if (!a || !b || !c) { printf("FAIL: malloc returned NULL in coalescing test\n"); return; }

    free(b);
    // Now b is free.
    void *d = malloc(100);
    printf("d=%p (should be == b if reused)\n", d);
    if (d == b) printf("PASS: Block reused\n");
    else printf("WARN: Block not reused (fragmentation)\n");

    free(a);
    free(c);
    if (d != b) free(d); else free(b); // d is either b or new.

    // Now a, b, c (or a, d, c) are free. If contiguous, they might coalesce.
    // Note: a, b, c were allocated sequentially.
    // If we free them all, we should get one large block.

    void *e = malloc(300);
    printf("e=%p (should be close to a)\n", e);
    // exact equality depends on overhead size, but if coalesced, it should fit where a was.
    // a was 100, b was 100, c was 100. Total 300 + 3*overhead.
    // malloc(300) should fit easily.
    if (e == a) printf("PASS: Coalesced large block reused\n");
    else printf("WARN: Coalescing failed or fragmentation\n");

    free(e);
}

void test_printf(void) {
    printf("[TEST] Testing printf...\n");
    printf("Int: %d, Hex: %x, Str: %s, Char: %c\n", 123, 0xABC, "test", 'X');
    printf("Padding (00042): %05d\n", 42);
    printf("Width (   42): %5d\n", 42);
    printf("Left (42   ): %-5d|\n", 42);
    printf("Pointer: %p\n", (void*)0x12345678);
    printf("Sign (+42): %+d\n", 42);
    printf("Space ( 42): % d\n", 42);
}

void test_stdlib(void) {
    printf("[TEST] Testing stdlib...\n");
    printf("atoi('123'): %d\n", atoi("123"));
    printf("atol('123456789'): %ld\n", atol("123456789"));
    srand(123);
    printf("rand(): %d\n", rand());
    printf("rand(): %d\n", rand());
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    printf("=== Libc Advanced Test ===\n");
    test_malloc();
    test_printf();
    test_stdlib();
    printf("=== Test Complete ===\n");
    return 0;
}
