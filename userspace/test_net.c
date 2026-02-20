#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    printf("Creating socket...\n");
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        printf("socket failed: %d\n", errno);
        return 1;
    }
    printf("Socket created: %d\n", fd);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(80);
    addr.sin_addr = htonl(0x7F000001); // 127.0.0.1
    memset(addr.sin_zero, 0, 8);

    printf("Connecting...\n");
    if (connect(fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("connect failed: %d (expected if no listener)\n", errno);
    } else {
        printf("Connected!\n");
    }

    /* Test sendto linkage (valid on connected socket if addr is NULL) */
    char buf[16] = "hello";
    ssize_t sent = sendto(fd, buf, 5, 0, NULL, 0);
    if (sent < 0) {
         printf("sendto failed: %d\n", errno);
    } else {
         printf("sendto sent %d bytes\n", (int)sent);
    }

    return 0;
}
