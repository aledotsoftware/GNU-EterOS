#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <termios.h>
#include <fcntl.h>
#include "sha256.h"

#define MAX_LINE 256

static int read_line(int fd, char *buf, int max_len) {
    int i = 0;
    char c;
    while (i < max_len - 1) {
        int r = read(fd, &c, 1);
        if (r < 0) { if (i == 0) return -1; break; }
        if (r == 0) break;
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

    if (getuid() != 0) {
        printf("Error: Only root can add users.\n");
        return 1;
    }

    const char* username = argv[1];

    struct termios term, term_orig;
    tcgetattr(0, &term_orig);
    term = term_orig;
    term.c_lflag &= ~ECHO;
    tcsetattr(0, TCSANOW, &term);

    printf("Password: ");
    fflush(stdout);
    char password[32];
    int len = read(0, password, sizeof(password) - 1);

    tcsetattr(0, TCSANOW, &term_orig);
    printf("\n");

    if (len <= 0) return 1;
    password[len] = '\0';
    if (password[len-1] == '\n') password[len-1] = '\0';

    /* Calculate SHA256 */
    uint8_t hash[SHA256_BLOCK_SIZE];
    sha256((const uint8_t*)password, strlen(password), hash);

    char hash_str[SHA256_BLOCK_SIZE * 2 + 1];
    for (int i = 0; i < SHA256_BLOCK_SIZE; i++) {
        snprintf(&hash_str[i*2], sizeof(hash_str) - (i * 2), "%02x", hash[i]);
    }

    /* Assign next UID/GID from /etc/passwd */
    int next_uid = 1000;

    int fd = open("/etc/passwd", O_RDONLY);
    if (fd >= 0) {
        char line[MAX_LINE];
        while (read_line(fd, line, sizeof(line)) > 0) {
            char user[32];
            user[0] = '\0';
            int uid = -1;

            int c_idx = 0;
            int field = 0;
            char uid_str[16];
            uid_str[0] = '\0';
            int u_idx = 0;

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
                }
            }

            if (strlen(uid_str) > 0) uid = atoi(uid_str);

            if (strcmp(user, username) == 0) {
                printf("Error: User '%s' already exists.\n", username);
                close(fd);
                return 1;
            }
            if (uid >= next_uid) {
                next_uid = uid + 1;
            }
        }
        close(fd);
    } else {
        fd = open("/etc/passwd", O_WRONLY | O_CREAT, 0644);
        if (fd >= 0) {
            const char* root_entry = "root:x:0:0:root:/root:/bin/sh\n";
            write(fd, root_entry, strlen(root_entry));
            close(fd);
        }
    }

    /* Append to /etc/passwd */
    fd = open("/etc/passwd", O_WRONLY | O_APPEND | O_CREAT, 0644);
    if (fd < 0) {
        printf("Error: Could not open /etc/passwd for writing\n");
        return 1;
    }
    char passwd_entry[MAX_LINE];
    snprintf(passwd_entry, sizeof(passwd_entry), "%s:x:%d:%d::/home/%s:/bin/sh\n", username, next_uid, next_uid, username);
    write(fd, passwd_entry, strlen(passwd_entry));
    close(fd);

    /* Check if shadow exists */
    int fd_shadow = open("/etc/shadow", O_RDONLY);
    if (fd_shadow < 0) {
        fd_shadow = open("/etc/shadow", O_WRONLY | O_CREAT, 0600);
        if (fd_shadow >= 0) {
            const char* root_entry = "root::19000:0:99999:7:::\n";
            write(fd_shadow, root_entry, strlen(root_entry));
            close(fd_shadow);
        }
    } else {
        close(fd_shadow);
    }

    /* Append to /etc/shadow */
    fd = open("/etc/shadow", O_WRONLY | O_APPEND | O_CREAT, 0600);
    if (fd < 0) {
        printf("Error: Could not open /etc/shadow for writing\n");
        return 1;
    }
    char shadow_entry[MAX_LINE];
    snprintf(shadow_entry, sizeof(shadow_entry), "%s:%s:19000:0:99999:7:::\n", username, hash_str);
    write(fd, shadow_entry, strlen(shadow_entry));
    close(fd);

    /* Enforce file permissions */
    chmod("/etc/shadow", 0600);
    chmod("/etc/passwd", 0644);

    printf("User '%s' created successfully.\n", username);
    return 0;
}
