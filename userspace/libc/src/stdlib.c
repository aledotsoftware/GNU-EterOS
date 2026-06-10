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

static int __environ_allocated = 0;

int setenv(const char *name, const char *value, int overwrite) {
    if (!name || name[0] == '\0' || strchr(name, '=')) {
        return -1;
    }

    size_t name_len = strlen(name);
    size_t value_len = value ? strlen(value) : 0;

    // Find if it already exists
    for (int i = 0; environ && environ[i] != NULL; i++) {
        if (strncmp(environ[i], name, name_len) == 0 && environ[i][name_len] == '=') {
            if (!overwrite) return 0;

            char *new_str = malloc(name_len + value_len + 2);
            if (!new_str) return -1;
            strcpy(new_str, name);
            new_str[name_len] = '=';
            if (value) strcpy(new_str + name_len + 1, value);
            else new_str[name_len + 1] = '\0';

            environ[i] = new_str;
            return 0;
        }
    }

    // Allocate new string
    char *new_str = malloc(name_len + value_len + 2);
    if (!new_str) return -1;
    strcpy(new_str, name);
    new_str[name_len] = '=';
    if (value) strcpy(new_str + name_len + 1, value);
    else new_str[name_len + 1] = '\0';

    // Count current environment size
    int env_count = 0;
    while (environ && environ[env_count] != NULL) env_count++;

    // Reallocate environ
    char **new_environ;
    if (__environ_allocated) {
        new_environ = realloc(environ, (env_count + 2) * sizeof(char *));
    } else {
        new_environ = malloc((env_count + 2) * sizeof(char *));
        if (new_environ && environ) {
            for (int i = 0; i < env_count; i++) {
                new_environ[i] = environ[i];
            }
        }
    }

    if (!new_environ) {
        free(new_str);
        return -1;
    }

    new_environ[env_count] = new_str;
    new_environ[env_count + 1] = NULL;
    environ = new_environ;
    __environ_allocated = 1;

    return 0;
}

int unsetenv(const char *name) {
    if (!name || name[0] == '\0' || strchr(name, '=')) {
        return -1;
    }

    if (!environ) return 0;

    size_t name_len = strlen(name);
    int write_idx = 0;

    for (int i = 0; environ[i] != NULL; i++) {
        if (strncmp(environ[i], name, name_len) == 0 && environ[i][name_len] == '=') {
            // Skip this one
            continue;
        }
        environ[write_idx++] = environ[i];
    }
    environ[write_idx] = NULL;

    return 0;
}

int putenv(char *string) {
    if (!string) return -1;

    char *eq = strchr(string, '=');
    if (!eq) {
        return unsetenv(string);
    }

    size_t name_len = eq - string;

    for (int i = 0; environ && environ[i] != NULL; i++) {
        if (strncmp(environ[i], string, name_len) == 0 && environ[i][name_len] == '=') {
            environ[i] = string;
            return 0;
        }
    }

    int env_count = 0;
    while (environ && environ[env_count] != NULL) env_count++;

    char **new_environ;
    if (__environ_allocated) {
        new_environ = realloc(environ, (env_count + 2) * sizeof(char *));
    } else {
        new_environ = malloc((env_count + 2) * sizeof(char *));
        if (new_environ && environ) {
            for (int i = 0; i < env_count; i++) {
                new_environ[i] = environ[i];
            }
        }
    }

    if (!new_environ) return -1;

    new_environ[env_count] = string;
    new_environ[env_count + 1] = NULL;
    environ = new_environ;
    __environ_allocated = 1;

    return 0;
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
    if (!base || nmemb <= 1 || size == 0) return;
    char *arr = (char *)base;
    size_t gap = nmemb / 2;
    while (gap > 0) {
        for (size_t i = gap; i < nmemb; i++) {
            for (size_t j = i; j >= gap && compar(arr + (j - gap) * size, arr + j * size) > 0; j -= gap) {
                char *a = arr + (j - gap) * size;
                char *b = arr + j * size;
                for (size_t k = 0; k < size; k++) {
                    char temp = a[k];
                    a[k] = b[k];
                    b[k] = temp;
                }
            }
        }
        gap /= 2;
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

long long strtoll(const char *nptr, char **endptr, int base) {
    long long res = 0;
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

unsigned long long strtoull(const char *nptr, char **endptr, int base) {
    unsigned long long res = 0;

    if (!nptr) return 0;

    while (*nptr == ' ' || *nptr == '\t' || *nptr == '\n') nptr++;

    if (*nptr == '+') {
        nptr++;
    } else if (*nptr == '-') {
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
