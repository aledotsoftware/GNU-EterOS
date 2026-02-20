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
            if (strcmp(argv[i], "--segfault") == 0) {
                printf("Testing signal handling (Segfault)...\n");

                /* This should cause SIGSEGV and task termination by kernel */
                volatile int *p = (int*)0;
                *p = 42;

                printf("This should NOT be printed if Segfault works.\n");
            }
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

    /* --- File I/O Tests --- */
    printf("\n=== Testing stdio File I/O ===\n");

    FILE *f = fopen("test.txt", "w");
    if (!f) {
        printf("Failed to open test.txt for writing (errno=%d)\n", errno);
    } else {
        printf("Opened test.txt for writing.\n");
        const char *text = "Hello, File!\n";
        size_t written = fwrite(text, 1, strlen(text), f);
        printf("Written %d bytes.\n", (int)written);
        fclose(f);
        printf("Closed file.\n");

        /* Read back */
        f = fopen("test.txt", "r");
        if (!f) {
            printf("Failed to open test.txt for reading (errno=%d)\n", errno);
        } else {
            printf("Opened test.txt for reading.\n");
            char buffer[32];
            memset(buffer, 0, sizeof(buffer));
            size_t read_bytes = fread(buffer, 1, sizeof(buffer)-1, f);
            printf("Read %d bytes: '%s'\n", (int)read_bytes, buffer);
            fclose(f);
        }
    }

    printf("\n=== Testing fseek/ftell ===\n");
    f = fopen("test_seek.txt", "w+");
    if (f) {
        fputs("1234567890", f);
        long pos = ftell(f);
        printf("Current pos after write: %ld (expected 10)\n", pos);

        fseek(f, 5, SEEK_SET);
        printf("Seek to 5.\n");
        fputc('X', f);

        fseek(f, 0, SEEK_SET); // rewind
        char buf[16];
        memset(buf, 0, 16);
        if (fgets(buf, 16, f)) {
            printf("Read back: '%s' (expected 12345X7890)\n", buf);
        } else {
            printf("fgets failed.\n");
        }

        fclose(f);
    } else {
         printf("Failed to open test_seek.txt (errno=%d)\n", errno);
    }

    return 0;
}
