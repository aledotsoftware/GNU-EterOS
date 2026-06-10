#ifndef _DIRENT_H
#define _DIRENT_H

#include <stdint.h>
#include <sys/types.h>

struct dirent {
    uint64_t d_ino;
    int64_t  d_off;
    unsigned short d_reclen;
    unsigned char  d_type;
    char           d_name[256];
};

typedef struct {
    int fd;
    char buf[4096];
    int buf_pos;
    int buf_end;
} DIR;

DIR *opendir(const char *name);
struct dirent *readdir(DIR *dirp);
void rewinddir(DIR *dirp);
int closedir(DIR *dirp);

#endif /* _DIRENT_H */
