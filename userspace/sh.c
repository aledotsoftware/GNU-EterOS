#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <errno.h>
#include <ctype.h>

#define MAX_CMD_LEN 256
#define MAX_ARGS 16

static char* trim_whitespace(char* s) {
    char* end;
    while (*s && isspace((unsigned char)*s)) s++;
    if (*s == '\0') return s;
    end = s + strlen(s) - 1;
    while (end > s && isspace((unsigned char)*end)) {
        *end = '\0';
        end--;
    }
    return s;
}

static int execute_command_line(char* line) {
    char* args[MAX_ARGS];
    int i = 0;

    line = trim_whitespace(line);
    if (line[0] == '\0') return 0;
    if (line[0] == '#') return 0;

    {
        char* comment = strchr(line, '#');
        if (comment) {
            *comment = '\0';
            line = trim_whitespace(line);
            if (line[0] == '\0') return 0;
        }
    }

    if (strcmp(line, "exit") == 0) return 1;

    if (strcmp(line, "help") == 0) {
        printf("Built-in commands:\n");
        printf("  help  - Show this message\n");
        printf("  exit  - Exit the shell\n");
        printf("  cd    - Change current directory\n");
        printf("  pwd   - Print current directory\n");
        printf("Other commands will be executed as external programs.\n");
        return 0;
    }

    {
        char* token = strtok(line, " ");
        while (token != NULL && i < MAX_ARGS - 1) {
            args[i++] = token;
            token = strtok(NULL, " ");
        }
    }
    args[i] = NULL;

    if (args[0] == NULL) return 0;

    if (strcmp(args[0], "cd") == 0) {
        const char *target = args[1] ? args[1] : "/";
        if (chdir(target) < 0) {
            printf("cd: %s: %d\n", target, errno);
        }
        return 0;
    }

    if (strcmp(args[0], "pwd") == 0) {
        char wd[128];
        if (getcwd(wd, sizeof(wd))) {
            printf("%s\n", wd);
        } else {
            printf("/\n");
        }
        return 0;
    }

    {
        int pid = fork();
        if (pid == 0) {
            if (execvp(args[0], args) < 0) {
                printf("Error: Command not found: %s. Type 'help' for built-in commands.\n", args[0]);
                exit(127);
            }
        } else if (pid > 0) {
            int st = 0;
            wait(&st);
        } else {
            printf("Error: Fork failed\n");
            return 1;
        }
    }

    return 0;
}

static int run_script_file(const char* path) {
    FILE* f = fopen(path, "r");
    char line[MAX_CMD_LEN];
    int lineno = 0;

    if (!f) {
        printf("sh: cannot open script %s: %d\n", path, errno);
        return 1;
    }

    while (fgets(line, sizeof(line), f)) {
        int rc;
        lineno++;
        rc = execute_command_line(line);
        if (rc == 1) break;
        if (rc != 0) {
            printf("sh: script error at line %d (%d)\n", lineno, rc);
        }
    }

    fclose(f);
    return 0;
}

int main(int argc, char *argv[]) {
    char cmd_buf[MAX_CMD_LEN];

    printf("Welcome to User Space Shell!\n");
    if (argc > 1) {
        return run_script_file(argv[1]);
    }

    while (1) {
        char cwd[128];
        if (getcwd(cwd, sizeof(cwd))) {
            printf("%s$ ", cwd);
        } else {
            printf("sh> ");
        }
        fflush(stdout);

        /* Read command */
        int len = read(0, cmd_buf, MAX_CMD_LEN - 1);
        if (len <= 0) break;

        cmd_buf[len] = 0;
        /* Remove newline */
        if (cmd_buf[len-1] == '\n') cmd_buf[len-1] = 0;
        if (cmd_buf[len-1] == '\r') cmd_buf[len-1] = 0;

        if (strlen(cmd_buf) == 0) continue;
        if (execute_command_line(cmd_buf) == 1) break;
    }

    return 0;
}
