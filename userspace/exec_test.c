#include <stdio.h>
#include <unistd.h>

int main(int argc, char* argv[]) {
    printf("[EXEC_TEST] Hello from exec_test!\n");
    for (int i = 0; i < argc; i++) {
        printf("[EXEC_TEST] argv[%d] = %s\n", i, argv[i]);
    }
    return 42;
}
