#include <errno.h>

int global_errno = 0;

int *__errno_location(void) {
    return &global_errno;
}
