#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#define MAX_CMD_LEN 256
#define MAX_ARGS 16

int main(int argc __attribute__((unused)), char *argv[] __attribute__((unused))) {
    char cmd_buf[MAX_CMD_LEN];
    char *args[MAX_ARGS];

    printf("Welcome to User Space Shell!\n");

    while (1) {
        printf("sh> ");
        fflush(stdout);

        /* Read command */
        int len = read(0, cmd_buf, MAX_CMD_LEN - 1);
        if (len <= 0) break;

        cmd_buf[len] = 0;
        /* Remove newline */
        if (cmd_buf[len-1] == '\n') cmd_buf[len-1] = 0;

        if (strlen(cmd_buf) == 0) continue;

        if (strcmp(cmd_buf, "exit") == 0) break;

        if (strcmp(cmd_buf, "help") == 0) {
            printf("Built-in commands:\n");
            printf("  help  - Show this message\n");
            printf("  exit  - Exit the shell\n");
            printf("Other commands will be executed as external programs.\n");
            continue;
        }

        /* Parse args */
        int i = 0;
        char *token = strtok(cmd_buf, " ");
        while (token != NULL && i < MAX_ARGS - 1) {
            args[i++] = token;
            token = strtok(NULL, " ");
        }
        args[i] = NULL;

        if (args[0] == NULL) continue;

        /* Execute */
        int pid = fork();
        if (pid == 0) {
            /* Child */
            if (execve(args[0], args, NULL) < 0) {
                printf("Error: Command not found: %s. Type 'help' for built-in commands.\n", args[0]);
                exit(1);
            }
        } else if (pid > 0) {
            /* Parent */
            wait(NULL);
        } else {
            printf("Error: Fork failed\n");
        }
    }

    return 0;
}
