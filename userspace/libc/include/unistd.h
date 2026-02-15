#ifndef _UNISTD_H
#define _UNISTD_H

#include <stdint.h>
#include <sys/syscall.h>

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

typedef int32_t ssize_t;
typedef uint64_t size_t;

/* File Descriptors */
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/* Flags for open() - Partial mapping to Kernel flags or just placeholders */
#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_CREAT     0x0040
#define O_EXCL      0x0080
#define O_TRUNC     0x0200
#define O_APPEND    0x0400

int open(const char *pathname, int flags);
int close(int fd);
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int64_t lseek(int fd, int64_t offset, int whence);
int getpid(void);
int kill(int pid, int sig);
unsigned int sleep(unsigned int seconds);

#endif /* _UNISTD_H */
