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

static int remove_user_from_file(const char* filepath, const char* username) {
    int fd = open(filepath, O_RDONLY);
    if (fd < 0) {
        return -1;
    }

    char temp_filepath[256];
    snprintf(temp_filepath, sizeof(temp_filepath), "%s.tmp", filepath);

    int temp_fd = open(temp_filepath, O_WRONLY | O_CREAT | O_TRUNC, 0600);
    if (temp_fd < 0) {
        close(fd);
        return -1;
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
        unlink(temp_filepath);
        return 0; // Not found, but no error during reading
    }

    unlink(filepath);
    rename(temp_filepath, filepath);

    return 1; // Found and removed
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

    int shadow_res = remove_user_from_file("/etc/shadow", username);
    if (shadow_res < 0) {
         printf("Error: Could not open /etc/shadow\n");
         return 1;
    }

    /* Enforce shadow file permissions */
    chmod("/etc/shadow", 0600);

    int passwd_res = remove_user_from_file("/etc/passwd", username);
    if (passwd_res < 0) {
         printf("Error: Could not open /etc/passwd\n");
         return 1;
    }
    /* Enforce passwd permissions */
    chmod("/etc/passwd", 0644);

    if (shadow_res == 0 && passwd_res == 0) {
        printf("Error: User '%s' not found.\n", username);
        return 1;
    }

    printf("User '%s' deleted successfully.\n", username);
    return 0;
}
