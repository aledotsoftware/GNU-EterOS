#include <stdlib.h>
#include <unistd.h>

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
