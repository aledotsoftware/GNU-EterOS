#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <sys/poll.h>
#include <sys/select.h>
#include <time.h>

static int g_failures = 0;

static void expect(int cond, const char* msg) {
    if (cond) {
        printf("[OK] %s\n", msg);
    } else {
        printf("[FAIL] %s (errno=%d)\n", msg, errno);
        g_failures++;
    }
}

static void test_openat_fstatat(void) {
    int dirfd;
    int fd;
    struct stat st;

    mkdir("/data", 0755);
    dirfd = open("/data", O_RDONLY | O_DIRECTORY);
    expect(dirfd >= 0, "open /data as dirfd");
    if (dirfd < 0) return;

    fd = openat(dirfd, "s1_file.txt", O_CREAT | O_RDWR | O_TRUNC, 0644);
    expect(fd >= 0, "openat create/trunc file");
    if (fd >= 0) {
        const char* msg = "abc";
        expect(write(fd, msg, 3) == 3, "write created file");
        close(fd);
    }

    fd = openat(dirfd, "s1_file.txt", O_CREAT | O_EXCL | O_RDWR, 0644);
    expect(fd < 0 && errno == EEXIST, "openat O_EXCL detects existing file");
    if (fd >= 0) close(fd);

    expect(fstatat(dirfd, "s1_file.txt", &st, 0) == 0, "fstatat relative path");
    if (fstatat(dirfd, "s1_file.txt", &st, 0) == 0) {
        expect(S_ISREG(st.st_mode), "fstatat returns regular file mode");
        expect(st.st_size >= 3, "fstatat returns non-zero size");
    }

    fd = openat(dirfd, "s1_file.txt", O_RDONLY, 0);
    if (fd >= 0) {
        expect(fstatat(fd, "", &st, AT_EMPTY_PATH) == 0, "fstatat AT_EMPTY_PATH on fd");
        close(fd);
    } else {
        expect(0, "openat existing file for AT_EMPTY_PATH");
    }

    fd = openat(dirfd, "s1_file.txt", O_RDONLY | O_DIRECTORY, 0);
    expect(fd < 0 && errno == ENOTDIR, "openat O_DIRECTORY rejects regular file");
    if (fd >= 0) close(fd);

    unlink("/data/s1_file.txt");
    close(dirfd);
}

static void test_fcntl_flags(void) {
    int fd = open("/README", O_RDONLY);
    int fl;
    int fdfl;
    expect(fd >= 0, "open /README for fcntl tests");
    if (fd < 0) return;

    fdfl = fcntl(fd, F_GETFD);
    expect(fdfl >= 0, "fcntl F_GETFD works");
    expect(fcntl(fd, F_SETFD, fdfl | FD_CLOEXEC) == 0, "fcntl F_SETFD FD_CLOEXEC");
    expect((fcntl(fd, F_GETFD) & FD_CLOEXEC) != 0, "fcntl F_GETFD reflects FD_CLOEXEC");

    fl = fcntl(fd, F_GETFL);
    expect(fl >= 0, "fcntl F_GETFL works");
    expect(fcntl(fd, F_SETFL, fl | O_NONBLOCK) == 0, "fcntl F_SETFL O_NONBLOCK");
    expect((fcntl(fd, F_GETFL) & O_NONBLOCK) != 0, "fcntl F_GETFL reflects O_NONBLOCK");

    close(fd);
}

static void test_poll_select(void) {
    int fd = open("/README", O_RDONLY);
    struct pollfd pfd;
    fd_set rfds;
    struct timeval tv;
    int rc;

    expect(fd >= 0, "open /README for poll/select");
    if (fd < 0) return;

    pfd.fd = fd;
    pfd.events = POLLIN;
    pfd.revents = 0;
    rc = poll(&pfd, 1, 0);
    expect(rc >= 1, "poll reports readable regular file");
    expect((pfd.revents & POLLIN) != 0, "poll revents has POLLIN");

    FD_ZERO(&rfds);
    FD_SET(fd, &rfds);
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    rc = select(fd + 1, &rfds, NULL, NULL, &tv);
    expect(rc >= 1, "select reports readable regular file");
    expect(FD_ISSET(fd, &rfds) != 0, "select readfds has fd set");

    close(fd);
}

static void test_clock_gettime_basic(void) {
    struct timespec t1, t2;
    expect(clock_gettime(CLOCK_REALTIME, &t1) == 0, "clock_gettime realtime");
    expect(clock_gettime(CLOCK_MONOTONIC, &t2) == 0, "clock_gettime monotonic");
    expect(t1.tv_nsec >= 0 && t1.tv_nsec < 1000000000L, "realtime nsec range");
    expect(t2.tv_nsec >= 0 && t2.tv_nsec < 1000000000L, "monotonic nsec range");
}

static void test_mprotect_mremap(void) {
    char* p = (char*)mmap(NULL, 4096, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    char* q;
    expect(p != MAP_FAILED, "mmap anonymous page");
    if (p == MAP_FAILED) return;

    p[0] = 'E';
    p[1] = 'T';
    p[2] = 'R';

    expect(mprotect(p, 4096, PROT_READ) == 0, "mprotect read-only");
    expect(mprotect(p, 4096, PROT_READ | PROT_WRITE) == 0, "mprotect read-write restore");

    q = (char*)mremap(p, 4096, 8192, MREMAP_MAYMOVE);
    expect(q != MAP_FAILED, "mremap grow mapping");
    if (q != MAP_FAILED) {
        expect(q[0] == 'E' && q[1] == 'T' && q[2] == 'R', "mremap preserves bytes");
        munmap(q, 8192);
    } else {
        munmap(p, 4096);
    }
}

int main(void) {
    printf("=== syscall sprint1 unit tests ===\n");
    test_openat_fstatat();
    test_fcntl_flags();
    test_poll_select();
    test_clock_gettime_basic();
    test_mprotect_mremap();

    if (g_failures == 0) {
        printf("RESULT: PASS\n");
        return 0;
    }
    printf("RESULT: FAIL (%d)\n", g_failures);
    return 1;
}
