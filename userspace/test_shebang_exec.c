#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

int main(void) {
    int pid;
    int st = 0;
    char *argv[] = {"/eter-shebang.sh", NULL};

    printf("=== shebang exec test ===\n");

    pid = fork();
    if (pid < 0) {
        printf("RESULT: FAIL (fork)\n");
        return 1;
    }
    if (pid == 0) {
        execv("/eter-shebang.sh", argv);
        printf("RESULT: FAIL (execv)\n");
        exit(127);
    }

    if (wait(&st) < 0) {
        printf("RESULT: FAIL (wait)\n");
        return 1;
    }

    if ((st & 0x7f) != 0) {
        printf("RESULT: FAIL (signal status=%d)\n", st);
        return 1;
    }
    if (((st >> 8) & 0xff) != 0) {
        printf("RESULT: FAIL (rc=%d)\n", (st >> 8) & 0xff);
        return 1;
    }

    printf("RESULT: PASS\n");
    return 0;
}
