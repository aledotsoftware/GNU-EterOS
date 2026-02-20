#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/wait.h>

void test_fork_exec() {
    printf("[TEST] Testing fork() and execve()...\n");
    int pid = fork();
    if (pid < 0) {
        printf("[TEST] fork() failed: %d\n", errno);
    } else if (pid == 0) {
        printf("[CHILD] In child process. Executing exec_test.elf...\n");
        char *argv[] = {"exec_test.elf", "arg1", "arg2", NULL};
        char *envp[] = {NULL};
        execve("/exec_test.elf", argv, envp);
        printf("[CHILD] execve failed: %d\n", errno);
        exit(1);
    } else {
        printf("[PARENT] Child PID: %d. Waiting...\n", pid);
        int status;
        waitpid(pid, &status, 0);
        printf("[PARENT] Child finished.\n");
    }
}

void test_networking() {
    printf("[TEST] Testing Networking (Userspace Sockets)...\n");

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        printf("[TEST] Error: socket() failed. Errno: %d\n", errno);
        return;
    }
    printf("[TEST] socket() success. FD: %d\n", sockfd);

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_port = htons(80);
    /* 8.8.8.8 */
    servaddr.sin_addr = htonl(0x08080808);

    printf("[TEST] Attempting connect to 8.8.8.8:80 (Timeout expected if no network)...\n");
    int ret = connect(sockfd, (struct sockaddr*)&servaddr, sizeof(servaddr));
    if (ret < 0) {
        printf("[TEST] connect() failed/timeout. Errno: %d\n", errno);
        /* Expected in test environment without gateway */
    } else {
        printf("[TEST] connect() success!\n");

        const char* msg = "GET / HTTP/1.1\r\nHost: google.com\r\n\r\n";
        ssize_t sent = send(sockfd, msg, strlen(msg), 0);
        printf("[TEST] sent: %d bytes\n", (int)sent);
    }

    close(sockfd);
    printf("[TEST] Socket closed.\n");
}

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    printf("userspace test starting...\n");

    test_fork_exec();
    test_networking();

    // Original test code preserved if needed, or simplified
    printf("Test complete.\n");
    return 0;
}
