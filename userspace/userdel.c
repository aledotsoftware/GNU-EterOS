#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#define MAX_LINE 256

static int read_line(int fd, char *buf, int max_len) {
    int i = 0;
    char c;
    while (i < max_len - 1) {
        int r = read(fd, &c, 1);
        if (r <= 0) break;
        buf[i++] = c;
        if (c == '\n') break;
    }
    buf[i] = '\0';
    return i;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <username>\n", argv[0]);
        return 1;
    }

    const char* username = argv[1];

    if (strcmp(username, "root") == 0) {
        printf("Error: Cannot delete 'root' user.\n");
        return 1;
    }

    int fd = open("/etc/shadow", O_RDONLY);
    if (fd < 0) {
        printf("Error: Could not open /etc/shadow\n");
        return 1;
    }

    int temp_fd = open("/etc/shadow.tmp", O_WRONLY | O_CREAT | O_TRUNC);
    if (temp_fd < 0) {
        printf("Error: Could not open /etc/shadow.tmp\n");
        close(fd);
        return 1;
    }

    int found = 0;
    char line[MAX_LINE];
    while (read_line(fd, line, sizeof(line)) > 0) {
        char user[32];
        user[0] = '\0';
        int c_idx = 0;

        for (int i = 0; line[i] != '\0'; i++) {
            if (line[i] == ':') break;
            if (c_idx < 31) {
                user[c_idx++] = line[i];
                user[c_idx] = '\0';
            }
        }

        if (strcmp(user, username) != 0) {
            write(temp_fd, line, strlen(line));
        } else {
            found = 1;
        }
    }

    close(fd);
    close(temp_fd);

    if (!found) {
        printf("Error: User '%s' not found.\n", username);
        unlink("/etc/shadow.tmp");
        return 1;
    }

    unlink("/etc/shadow");
    rename("/etc/shadow.tmp", "/etc/shadow");

    printf("User '%s' deleted successfully.\n", username);
    return 0;
}
