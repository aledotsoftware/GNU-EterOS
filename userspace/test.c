#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    printf("Hello from Userspace Libc!\n");

    int pid = getpid();
    printf("My PID is: %d\n", pid);

    printf("Opening /dev/tty...\n");
    int fd = open("/dev/tty", O_WRONLY);
    if (fd < 0) {
        printf("Failed to open /dev/tty, errno=%d\n", errno);
    } else {
        printf("Writing to /dev/tty (FD=%d)...\n", fd);
        const char *msg = "This is written to /dev/tty via syscall!\n";
        write(fd, msg, 42); /* Length hardcoded for simplicity */
        close(fd);
        printf("Closed /dev/tty.\n");
    }

    printf("Testing signal handling (Segfault)...\n");

    /* This should cause SIGSEGV and task termination by kernel */
    volatile int *p = (int*)0;
    *p = 42;

    printf("This should NOT be printed if Segfault works.\n");

    return 0;
}
