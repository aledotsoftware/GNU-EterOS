#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "sha256.h"

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
    if (argc != 3) {
        printf("Usage: %s <username> <new_password>\n", argv[0]);
        return 1;
    }

    const char* username = argv[1];
    const char* password = argv[2];

    /* Calculate SHA256 */
    uint8_t hash[SHA256_BLOCK_SIZE];
    sha256((const uint8_t*)password, strlen(password), hash);

    char hash_str[SHA256_BLOCK_SIZE * 2 + 1];
    for (int i = 0; i < SHA256_BLOCK_SIZE; i++) {
        snprintf(&hash_str[i*2], sizeof(hash_str) - (i * 2), "%02x", hash[i]);
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
        char uid_str[16];
        uid_str[0] = '\0';
        char gid_str[16];
        gid_str[0] = '\0';

        int field = 0;
        int c_idx = 0;
        int u_idx = 0;
        int g_idx = 0;

        for (int i = 0; line[i] != '\0'; i++) {
            if (line[i] == ':') {
                field++;
                continue;
            }
            if (field == 0 && c_idx < 31) {
                user[c_idx++] = line[i];
                user[c_idx] = '\0';
            } else if (field == 2 && u_idx < 15) {
                uid_str[u_idx++] = line[i];
                uid_str[u_idx] = '\0';
            } else if (field == 3 && g_idx < 15) {
                if (line[i] != '\n') {
                    gid_str[g_idx++] = line[i];
                    gid_str[g_idx] = '\0';
                }
            }
        }

        if (strcmp(user, username) == 0) {
            found = 1;
            char entry[MAX_LINE];
            snprintf(entry, sizeof(entry), "%s:%s:%s:%s\n", user, hash_str, uid_str, gid_str);
            write(temp_fd, entry, strlen(entry));
        } else {
            write(temp_fd, line, strlen(line));
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

    printf("Password for user '%s' changed successfully.\n", username);
    return 0;
}
