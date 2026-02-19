#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char* argv[], char* envp[]) {
    printf("\n=== EXEC_TEST START ===\n");
    printf("Hello from exec_test! PID: %d\n", getpid());
    printf("argc: %d\n", argc);

    for (int i = 0; i < argc; i++) {
        printf("argv[%d]: %s\n", i, argv[i]);
    }

    printf("Environment:\n");
    if (envp) {
        for (int i = 0; envp[i] != NULL; i++) {
            printf("envp[%d]: %s\n", i, envp[i]);
        }
    } else {
        printf("(null)\n");
    }

    printf("=== EXEC_TEST END ===\n");
    return 0;
}
