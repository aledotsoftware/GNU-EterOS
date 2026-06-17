#include <dirent.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/syscall.h>

extern long syscall(long nr, ...);

DIR *opendir(const char *name) {
    if (!name) {
        errno = ENOENT;
        return NULL;
    }

    int fd = syscall(SYS_open, name, O_RDONLY, 0);
    if (fd < 0) {
        errno = (int)(-fd);
        return NULL;
    }

    DIR *dir = (DIR *)malloc(sizeof(DIR));
    if (!dir) {
        syscall(SYS_close, fd);
        errno = ENOMEM;
        return NULL;
    }

    dir->fd = fd;
    dir->buf_pos = 0;
    dir->buf_end = 0;

    return dir;
}

struct dirent *readdir(DIR *dirp) {
    if (!dirp) {
        errno = EBADF;
        return NULL;
    }

    if (dirp->buf_pos >= dirp->buf_end) {
        long ret = syscall(SYS_getdents64, dirp->fd, dirp->buf, sizeof(dirp->buf));
        if (ret <= 0) {
            if ((unsigned long)ret >= (unsigned long)-4095) errno = (int)(-ret);
            return NULL; // EOF or error
        }
        dirp->buf_end = ret;
        dirp->buf_pos = 0;
    }

    // SYS_getdents64 returns linux_dirent64 structures.
    // Our struct dirent in dirent.h is:
    // uint64_t d_ino;
    // int64_t  d_off;
    // unsigned short d_reclen;
    // unsigned char  d_type;
    // char           d_name[256];
    // This exactly matches linux_dirent64 struct layout.

    struct dirent *ent = (struct dirent *)(dirp->buf + dirp->buf_pos);
    dirp->buf_pos += ent->d_reclen;

    return ent;
}

void rewinddir(DIR *dirp) {
    if (dirp) {
        syscall(SYS_lseek, dirp->fd, 0, SEEK_SET);
        dirp->buf_pos = 0;
        dirp->buf_end = 0;
    }
}

int closedir(DIR *dirp) {
    if (!dirp) {
        errno = EBADF;
        return -1;
    }

    int ret = syscall(SYS_close, dirp->fd);
    free(dirp);

    if ((unsigned long)ret >= (unsigned long)-4095) {
        errno = (int)(-ret);
        return -1;
    }
    return 0;
}
