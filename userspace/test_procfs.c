#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

static int read_text_file(const char* path, char* out, int out_sz) {
    int fd = open(path, O_RDONLY, 0);
    int n;
    if (fd < 0) return -1;
    n = (int)read(fd, out, (size_t)(out_sz - 1));
    close(fd);
    if (n < 0) return -1;
    out[n] = '\0';
    return n;
}

static int expect_contains(const char* hay, const char* needle, const char* what) {
    if (!strstr(hay, needle)) {
        printf("[FAIL] %s (missing: %s)\n", what, needle);
        return 1;
    }
    printf("[OK] %s\n", what);
    return 0;
}

int main(void) {
    int fails = 0;
    char buf[1024];

    printf("=== procfs unit test ===\n");

    if (read_text_file("/proc/self/status", buf, sizeof(buf)) < 0) {
        printf("[FAIL] read /proc/self/status (errno=%d)\n", errno);
        return 1;
    }
    fails += expect_contains(buf, "Name:\t", "status has Name");
    fails += expect_contains(buf, "State:\t", "status has State");
    fails += expect_contains(buf, "Pid:\t", "status has Pid");
    fails += expect_contains(buf, "PPid:\t", "status has PPid");
    fails += expect_contains(buf, "Uid:\t", "status has Uid");
    fails += expect_contains(buf, "Gid:\t", "status has Gid");

    if (read_text_file("/proc/self/stat", buf, sizeof(buf)) < 0) {
        printf("[FAIL] read /proc/self/stat (errno=%d)\n", errno);
        return 1;
    }
    fails += expect_contains(buf, "(", "stat has comm start");
    fails += expect_contains(buf, ")", "stat has comm end");
    fails += expect_contains(buf, " ", "stat has fields");

    if (read_text_file("/proc/1/status", buf, sizeof(buf)) < 0) {
        printf("[FAIL] read /proc/1/status (errno=%d)\n", errno);
        fails++;
    } else {
        fails += expect_contains(buf, "Pid:\t", "pid namespace lookup works");
    }

    if (fails == 0) {
        printf("RESULT: PASS\n");
        return 0;
    }

    printf("RESULT: FAIL (%d)\n", fails);
    return 1;
}
