#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

int main(int argc, char **argv) {
    printf("Hello from Userspace Libc!\n");

    printf("Received argc: %d\n", argc);
    if (argc > 0 && argv != NULL) {
        for (int i = 0; i < argc; i++) {
            printf("argv[%d] = '%s'\n", i, argv[i]);
        }

        /* Verify argv termination */
        if (argv[argc] == NULL) {
            printf("argv[%d] is NULL (Correct)\n", argc);
        } else {
            printf("argv[%d] is NOT NULL (Incorrect)\n", argc);
        }
    } else {
        printf("Error: argv is NULL or argc <= 0\n");
    }

    int pid = getpid();
    printf("My PID is: %d\n", pid);

    printf("Opening /dev/tty...\n");
    int fd = open("/dev/tty", O_WRONLY);
    if (fd < 0) {
        printf("Failed to open /dev/tty, errno=%d\n", errno);
    } else {
        printf("Writing to /dev/tty (FD=%d)...\n", fd);
        const char *msg = "This is written to /dev/tty via syscall!\n";
        write(fd, msg, strlen(msg));
        close(fd);
        printf("Closed /dev/tty.\n");
    }

    /*
    printf("Testing signal handling (Segfault)...\n");
    volatile int *p = (int*)0;
    *p = 42;
    printf("This should NOT be printed if Segfault works.\n");
    */

    return 0;
}
