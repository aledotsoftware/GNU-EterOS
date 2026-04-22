#include <stdio.h>
#include <dlfcn.h>
#include <string.h>

static int g_fail = 0;

static void expect(int cond, const char *msg) {
    if (cond) {
        printf("[OK] %s\n", msg);
    } else {
        printf("[FAIL] %s\n", msg);
        g_fail = 1;
    }
}

int main(void) {
    void *h_self;
    void *h_file;
    void *sym;
    char *err;

    printf("=== dlopen phase1 test ===\n");

    h_self = dlopen(NULL, RTLD_NOW);
    expect(h_self != NULL, "dlopen(NULL) returns self handle");

    sym = dlsym(RTLD_DEFAULT, "printf");
    expect(sym != NULL, "dlsym(RTLD_DEFAULT, printf)");

    h_file = dlopen("/README", RTLD_NOW);
    expect(h_file != NULL, "dlopen existing file as phase1 handle");
    if (h_file) {
        sym = dlsym(h_file, "printf");
        expect(sym != NULL, "dlsym(handle, printf) builtin symbol");
        expect(dlclose(h_file) == 0, "dlclose(handle)");
    }

    sym = dlsym(RTLD_DEFAULT, "symbol_that_does_not_exist");
    expect(sym == NULL, "dlsym unknown symbol returns NULL");
    err = dlerror();
    expect(err != NULL && strstr(err, "symbol not found") != NULL, "dlerror returns symbol-not-found message");

    h_file = dlopen("/this/path/does/not/exist.so", RTLD_NOW);
    expect(h_file == NULL, "dlopen missing file returns NULL");
    err = dlerror();
    expect(err != NULL && strstr(err, "file not found") != NULL, "dlerror returns file-not-found message");

    if (g_fail) {
        printf("RESULT: FAIL\n");
        return 1;
    }

    printf("RESULT: PASS\n");
    return 0;
}
