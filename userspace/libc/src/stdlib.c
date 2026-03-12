#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#define ATEXIT_MAX 32
static void (*atexit_funcs[ATEXIT_MAX])(void);
static int atexit_count = 0;

int atexit(void (*func)(void)) {
    if (atexit_count >= ATEXIT_MAX) return -1;
    atexit_funcs[atexit_count++] = func;
    return 0;
}

void __run_atexit(void) {
    for (int i = atexit_count - 1; i >= 0; i--) {
        if (atexit_funcs[i]) atexit_funcs[i]();
    }
}

int system(const char *command) {
    if (!command) return 1;

    int pid = fork();
    if (pid < 0) return -1;

    if (pid == 0) {
        // Child
        char *argv[] = {"sh", "-c", (char *)command, NULL};
        execve("/bin/sh", argv, environ);
        exit(127);
    } else {
        // Parent
        int status;
        waitpid(pid, &status, 0);
        return status;
    }
}

char **environ;

char *getenv(const char *name) {
    if (!environ || !name) return NULL;
    size_t len = strlen(name);
    for (int i = 0; environ[i] != NULL; i++) {
        if (strncmp(environ[i], name, len) == 0 && environ[i][len] == '=') {
            return environ[i] + len + 1;
        }
    }
    return NULL;
}

int setenv(const char *name, const char *value, int overwrite) {
    (void)name;
    (void)value;
    (void)overwrite;
    // Simple mock, as proper setenv requires reallocating environ array.
    // Full implementation requires mallocing and managing memory which is out of scope for stub.
    return -1;
}

void abort(void) {
    exit(134); // SIGABRT
}

static unsigned int next = 1;

int rand(void) {
    next = next * 1103515245 + 12345;
    return (unsigned int)(next/65536) % 32768;
}

void srand(unsigned int seed) {
    next = seed;
}

int atoi(const char *nptr) {
    int res = 0;
    int sign = 1;

    if (!nptr) return 0;

    while (*nptr == ' ' || *nptr == '\t' || *nptr == '\n') nptr++;

    if (*nptr == '-') {
        sign = -1;
        nptr++;
    } else if (*nptr == '+') {
        nptr++;
    }

    while (*nptr >= '0' && *nptr <= '9') {
        res = res * 10 + (*nptr - '0');
        nptr++;
    }
    return res * sign;
}

void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    // Simple insertion sort for now
    char *arr = (char *)base;
    char temp[size]; // VLA, size should be small enough
    for (size_t i = 1; i < nmemb; i++) {
        for (size_t j = i; j > 0 && compar(arr + (j - 1) * size, arr + j * size) > 0; j--) {
            // Swap arr[j-1] and arr[j]
            char *a = arr + (j - 1) * size;
            char *b = arr + j * size;
            for (size_t k = 0; k < size; k++) {
                temp[k] = a[k];
                a[k] = b[k];
                b[k] = temp[k];
            }
        }
    }
}

void *bsearch(const void *key, const void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    const char *arr = (const char *)base;
    size_t left = 0;
    size_t right = nmemb;
    while (left < right) {
        size_t mid = left + (right - left) / 2;
        int cmp = compar(key, arr + mid * size);
        if (cmp < 0) {
            right = mid;
        } else if (cmp > 0) {
            left = mid + 1;
        } else {
            return (void *)(arr + mid * size);
        }
    }
    return NULL;
}

int abs(int j) {
    return j < 0 ? -j : j;
}

long strtol(const char *nptr, char **endptr, int base) {
    long res = 0;
    int sign = 1;

    if (!nptr) return 0;

    while (*nptr == ' ' || *nptr == '\t' || *nptr == '\n') nptr++;

    if (*nptr == '-') {
        sign = -1;
        nptr++;
    } else if (*nptr == '+') {
        nptr++;
    }

    if (base == 0) {
        if (*nptr == '0') {
            if (*(nptr + 1) == 'x' || *(nptr + 1) == 'X') {
                base = 16;
                nptr += 2;
            } else {
                base = 8;
            }
        } else {
            base = 10;
        }
    } else if (base == 16) {
        if (*nptr == '0' && (*(nptr + 1) == 'x' || *(nptr + 1) == 'X')) {
            nptr += 2;
        }
    }

    while (1) {
        int v = -1;
        if (*nptr >= '0' && *nptr <= '9') v = *nptr - '0';
        else if (*nptr >= 'a' && *nptr <= 'z') v = *nptr - 'a' + 10;
        else if (*nptr >= 'A' && *nptr <= 'Z') v = *nptr - 'A' + 10;

        if (v < 0 || v >= base) break;

        res = res * base + v;
        nptr++;
    }

    if (endptr) *endptr = (char *)nptr;

    return res * sign;
}

unsigned long strtoul(const char *nptr, char **endptr, int base) {
    unsigned long res = 0;

    if (!nptr) return 0;

    while (*nptr == ' ' || *nptr == '\t' || *nptr == '\n') nptr++;

    if (*nptr == '+') {
        nptr++;
    } else if (*nptr == '-') {
        /* strtoul handles negative by returning the wrapped unsigned value */
        nptr++;
        // Ignoring negative handling for simplicity, though standard requires it.
    }

    if (base == 0) {
        if (*nptr == '0') {
            if (*(nptr + 1) == 'x' || *(nptr + 1) == 'X') {
                base = 16;
                nptr += 2;
            } else {
                base = 8;
            }
        } else {
            base = 10;
        }
    } else if (base == 16) {
        if (*nptr == '0' && (*(nptr + 1) == 'x' || *(nptr + 1) == 'X')) {
            nptr += 2;
        }
    }

    while (1) {
        int v = -1;
        if (*nptr >= '0' && *nptr <= '9') v = *nptr - '0';
        else if (*nptr >= 'a' && *nptr <= 'z') v = *nptr - 'a' + 10;
        else if (*nptr >= 'A' && *nptr <= 'Z') v = *nptr - 'A' + 10;

        if (v < 0 || v >= base) break;

        res = res * base + v;
        nptr++;
    }

    if (endptr) *endptr = (char *)nptr;

    return res;
}

long atol(const char *nptr) {
    long res = 0;
    int sign = 1;

    if (!nptr) return 0;

    while (*nptr == ' ' || *nptr == '\t' || *nptr == '\n') nptr++;

    if (*nptr == '-') {
        sign = -1;
        nptr++;
    } else if (*nptr == '+') {
        nptr++;
    }

    while (*nptr >= '0' && *nptr <= '9') {
        res = res * 10 + (*nptr - '0');
        nptr++;
    }
    return res * sign;
}
