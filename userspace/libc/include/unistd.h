#ifndef _UNISTD_H
#define _UNISTD_H

#include <stdint.h>
#include <stdarg.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <fcntl.h>

#ifndef SEEK_SET
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2
#endif

/* File Descriptors */
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

/* access() mode flags */
#define F_OK    0
#define R_OK    4
#define W_OK    2
#define X_OK    1

/* File I/O */
int open(const char *pathname, int flags, ...);
int creat(const char *pathname, mode_t mode);
int close(int fd);
ssize_t read(int fd, void *buf, size_t count);
ssize_t write(int fd, const void *buf, size_t count);
int64_t lseek(int fd, int64_t offset, int whence);
int mkdir(const char *pathname, mode_t mode);
int unlink(const char *pathname);
int rmdir(const char *pathname);
int ftruncate(int fd, int64_t length);

/* Process */
int getpid(void);
int getppid(void);
uid_t getuid(void);
gid_t getgid(void);
uid_t geteuid(void);
gid_t getegid(void);
pid_t setsid(void);
int setpgid(pid_t pid, pid_t pgid);
pid_t getpgrp(void);
int tcgetpgrp(int fd);
int tcsetpgrp(int fd, pid_t pgrp);
int setuid(uid_t uid);
int setgid(gid_t gid);
int kill(int pid, int sig);
int fork(void);
int execve(const char *pathname, char *const argv[], char *const envp[]);
int execv(const char *pathname, char *const argv[]);
int execvp(const char *file, char *const argv[]);
int execvpe(const char *file, char *const argv[], char *const envp[]);
void exit(int status);

/* File Descriptors */
int pipe(int pipefd[2]);
int pipe2(int pipefd[2], int flags);
int dup(int oldfd);
int dup2(int oldfd, int newfd);
int dup3(int oldfd, int newfd, int flags);
int access(const char *pathname, int mode);
int chmod(const char *pathname, mode_t mode);
int faccessat(int dirfd, const char *pathname, int mode, int flags);
int chdir(const char *pathname);
ssize_t readlink(const char *pathname, char *buf, size_t bufsiz);
ssize_t readlinkat(int dirfd, const char *pathname, char *buf, size_t bufsiz);
int isatty(int fd);

/* Memory */
void *brk(void *addr);
void *sbrk(int64_t increment);

/* Scheduling */
int sched_yield(void);
unsigned int sleep(unsigned int seconds);
int usleep(unsigned int usec);

/* Misc */
char *getcwd(char *buf, size_t size);
int reboot(int magic, int magic2, int cmd, void *arg);

/* getopt */
extern char *optarg;
extern int optind;
extern int opterr;
extern int optopt;
int getopt(int argc, char * const argv[], const char *optstring);

int execveat(int dirfd, const char *pathname, char *const argv[], char *const envp[], int flags);

#endif /* _UNISTD_H */
