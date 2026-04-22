#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/epoll.h>

static int fails = 0;

static void check(int cond, const char* msg) {
    if (cond) {
        printf("[OK] %s\n", msg);
    } else {
        printf("[FAIL] %s (errno=%d)\n", msg, errno);
        fails++;
    }
}

int main(void) {
    int epfd;
    int fd;
    struct epoll_event ev;
    struct epoll_event out[4];
    int n;

    printf("=== epoll test ===\n");

    epfd = epoll_create1(0);
    check(epfd >= 0, "epoll_create1");
    if (epfd < 0) return 1;

    fd = open("/README", O_RDONLY);
    check(fd >= 0, "open /README");
    if (fd < 0) {
        close(epfd);
        return 1;
    }

    ev.events = EPOLLIN;
    ev.data.fd = fd;
    check(epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) == 0, "epoll_ctl ADD");
    check(epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) < 0 && errno == EEXIST, "epoll_ctl ADD duplicate -> EEXIST");

    ev.events = EPOLLIN | EPOLLOUT;
    ev.data.u64 = 0x1234;
    check(epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev) == 0, "epoll_ctl MOD");

    n = epoll_wait(epfd, out, 4, 0);
    check(n >= 1, "epoll_wait immediate event");
    if (n >= 1) {
        check((out[0].events & EPOLLIN) != 0, "epoll_wait returned EPOLLIN");
    }

    check(epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL) == 0, "epoll_ctl DEL");
    check(epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL) < 0 && errno == ENOENT, "epoll_ctl DEL missing -> ENOENT");

    n = epoll_wait(epfd, out, 4, 0);
    check(n == 0, "epoll_wait after DEL has no events");

    close(fd);
    close(epfd);

    if (fails == 0) {
        printf("RESULT: PASS\n");
        return 0;
    }
    printf("RESULT: FAIL (%d)\n", fails);
    return 1;
}
