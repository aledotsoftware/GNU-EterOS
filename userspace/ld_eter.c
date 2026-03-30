#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

int main(int argc, char *argv[]) {
    char *target_argv[64];
    char *target_env[64];
    int ai = 0;
    int ei = 0;

    printf("[ld-eter] dynamic loader stub\n");
    if (argc < 2) {
        printf("[ld-eter] missing target executable\n");
        return 127;
    }

    printf("[ld-eter] target: %s\n", argv[1]);

    target_argv[ai++] = argv[1];
    for (int i = 2; i < argc && ai < (int)(sizeof(target_argv) / sizeof(target_argv[0])) - 1; i++) {
        target_argv[ai++] = argv[i];
    }
    target_argv[ai] = NULL;

    if (environ) {
        for (int i = 0; environ[i] != NULL && ei < (int)(sizeof(target_env) / sizeof(target_env[0])) - 2; i++) {
            if (strcmp(environ[i], "ETER_SKIP_PT_INTERP=1") == 0) continue;
            target_env[ei++] = environ[i];
        }
    }
    target_env[ei++] = "ETER_SKIP_PT_INTERP=1";
    target_env[ei] = NULL;

    execve(argv[1], target_argv, target_env);
    printf("[ld-eter] execve failed: %d\n", errno);
    return 127;
}
