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
    (void)argc;
    (void)argv;

    /* Initialize /etc/shadow if missing */
    int shadow_fd = open("/etc/shadow", O_RDONLY);
    if (shadow_fd < 0) {
        shadow_fd = open("/etc/shadow", O_WRONLY | O_CREAT);
        if (shadow_fd >= 0) {
            const char* root_entry = "root::0:0\n";
            write(shadow_fd, root_entry, strlen(root_entry));
            close(shadow_fd);
        }
    } else {
        close(shadow_fd);
    }

    /* Check autologin */
    int auto_fd = open("/etc/autologin", O_RDONLY);
    if (auto_fd >= 0) {
        char auto_buf[2];
        int len = read(auto_fd, auto_buf, 1);
        close(auto_fd);
        if (len > 0 && auto_buf[0] == '1') {
            /* Auto-login as root (UID 0) */
            if (setuid(0) < 0 || setgid(0) < 0) {
                printf("Error: Failed to drop privileges for autologin\n");
                return 1;
            }
            char *args[] = {"sh", NULL};
            execve("sh.elf", args, NULL);
            return 1;
        }
    }

    while (1) {
        char username[32];
        char password[32];

        printf("eterOS login: ");
        fflush(stdout);
        int len = read(0, username, sizeof(username) - 1);
        if (len <= 0) continue;
        username[len] = '\0';
        if (username[len-1] == '\n') username[len-1] = '\0';
        if (strlen(username) == 0) continue;

        printf("Password: ");
        fflush(stdout);
        len = read(0, password, sizeof(password) - 1);
        if (len <= 0) continue;
        password[len] = '\0';
        if (password[len-1] == '\n') password[len-1] = '\0';

        /* Read /etc/shadow to authenticate */
        int fd = open("/etc/shadow", O_RDONLY);
        if (fd < 0) {
            printf("Error: Could not open /etc/shadow\n");
            continue;
        }

        /* Compute SHA256 of entered password */
        char hash_str[SHA256_BLOCK_SIZE * 2 + 1];
        if (strlen(password) > 0) {
            uint8_t hash[SHA256_BLOCK_SIZE];
            sha256((const uint8_t*)password, strlen(password), hash);
            for (int i = 0; i < SHA256_BLOCK_SIZE; i++) {
                snprintf(&hash_str[i*2], sizeof(hash_str) - (i * 2), "%02x", hash[i]);
            }
        } else {
            hash_str[0] = '\0';
        }

        int found = 0;
        char line[MAX_LINE];
        while (read_line(fd, line, sizeof(line)) > 0) {
            char user[32];
            user[0] = '\0';
            char stored_hash[65];
            stored_hash[0] = '\0';
            char uid_str[16];
            uid_str[0] = '\0';
            char gid_str[16];
            gid_str[0] = '\0';

            int field = 0;
            int c_idx = 0;
            int h_idx = 0;
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
                } else if (field == 1 && h_idx < 64) {
                    stored_hash[h_idx++] = line[i];
                    stored_hash[h_idx] = '\0';
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

            int uid = strlen(uid_str) > 0 ? atoi(uid_str) : -1;
            int gid = strlen(gid_str) > 0 ? atoi(gid_str) : -1;

            if (strcmp(user, username) == 0) {
                found = 1;
                /* Check hash or empty hash */
                if ((strlen(stored_hash) == 0 && strlen(password) == 0) ||
                    (strcmp(stored_hash, hash_str) == 0)) {

                    printf("\nWelcome to eterOS, %s!\n", username);
                    close(fd);
                    if (setgid(gid) < 0 || setuid(uid) < 0) {
                        printf("Error: Failed to drop privileges\n");
                        exit(1);
                    }
                    char *args[] = {"sh", NULL};
                    execve("sh.elf", args, NULL);

                    /* Should not return */
                    printf("Failed to execute shell.\n");
                    exit(1);
                }
                break;
            }
        }

        close(fd);

        if (!found || (found && ((strlen(password) > 0) || (strlen(password) == 0)))) {
            printf("\nLogin incorrect.\n");
        }
    }

    return 0;
}
