#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

extern uint16_t htons(uint16_t v);

int main() {
    printf("[ECHO] Starting server on port 8080...\n");
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        printf("[ECHO] Error creating socket\n");
        return 1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr = 0; /* INADDR_ANY */

    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("[ECHO] Error binding\n");
        return 1;
    }

    if (listen(server_fd, 3) < 0) {
        printf("[ECHO] Error listening\n");
        return 1;
    }

    printf("[ECHO] Listening on port 8080. Waiting for connections...\n");

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            printf("[ECHO] Accept failed\n");
            continue;
        }

        printf("[ECHO] Client connected! FD: %d\n", client_fd);

        char buf[128];
        while (1) {
            int n = recv(client_fd, buf, sizeof(buf)-1, 0);
            if (n <= 0) break;
            buf[n] = 0;
            printf("[ECHO] Received: %s\n", buf);
            send(client_fd, buf, n, 0);
        }

        printf("[ECHO] Client disconnected.\n");
        close(client_fd);
    }
    return 0;
}
