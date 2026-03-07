#ifndef _UNISTD_H
#define _UNISTD_H

#include <stdint.h>
#include <sys/syscall.h>
#include <sys/types.h>

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

/* File Descriptors */
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/* Flags for open() */
#define O_RDONLY    0x0000
#define O_WRONLY    0x0001
#define O_RDWR      0x0002
#define O_CREAT     0x0040
#define O_EXCL      0x0080
#define O_TRUNC     0x0200
#define O_APPEND    0x0400

/* access() mode flags */
#define F_OK    0
#define R_OK    4
#define W_OK    2
#define X_OK    1

/* File I/O */
int open(const char *pathname, int flags);
int close(int fd);
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int64_t lseek(int fd, int64_t offset, int whence);
int unlink(const char *pathname);
int rmdir(const char *pathname);

/* Process */
int getpid(void);
int getppid(void);
int setuid(int uid);
int setgid(int gid);
int kill(int pid, int sig);
int fork(void);
int execve(const char *pathname, char *const argv[], char *const envp[]);
void exit(int status);

/* File Descriptors */
int pipe(int pipefd[2]);
int dup2(int oldfd, int newfd);
int access(const char *pathname, int mode);

/* Memory */
void *brk(void *addr);
void *sbrk(int64_t increment);

/* Scheduling */
int sched_yield(void);
unsigned int sleep(unsigned int seconds);
int usleep(unsigned int usec);

/* Misc */
char *getcwd(char *buf, size_t size);

#endif /* _UNISTD_H */
