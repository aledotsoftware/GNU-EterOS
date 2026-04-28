#include <stdio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdint.h>

#define OTA_MAX_URL 256

/* Mock string functions since we test natively */
size_t strlcpy(char *dst, const char *src, size_t dsize) {
    const char *osrc = src;
    size_t nleft = dsize;
    if (nleft != 0) {
        while (--nleft != 0) {
            if ((*dst++ = *src++) == '\0') break;
        }
    }
    if (nleft == 0) {
        if (dsize != 0) *dst = '\0';
        while (*src++) ;
    }
    return(src - osrc - 1);
}

static uint16_t parse_url(const char* url, char* host, size_t host_size, char* path, size_t path_size) {
    uint16_t port = 80;
    if (strncmp(url, "http://", 7) == 0) {
        url += 7;
    } else if (strncmp(url, "https://", 8) == 0) {
        url += 8;
        port = 443;
    }

    const char* slash = strchr(url, '/');
    if (slash) {
        size_t host_len = slash - url;
        if (host_len >= host_size) {
            host_len = host_size - 1;
        }
        memcpy(host, url, host_len);
        host[host_len] = '\0';
        strlcpy(path, slash, path_size);
    } else {
        strlcpy(host, url, host_size);
        strlcpy(path, "/", path_size);
    }
    return port;
}

int main() {
    char host[256];
    char path[256];

    /* Test 1: Simple HTTP URL */
    uint16_t port1 = parse_url("http://repo.eteros.org/updates/x86_64", host, sizeof(host), path, sizeof(path));
    assert(port1 == 80);
    assert(strcmp(host, "repo.eteros.org") == 0);
    assert(strcmp(path, "/updates/x86_64") == 0);

    /* Test 2: HTTPS URL */
    uint16_t port2 = parse_url("https://secure.eteros.org/latest.bin", host, sizeof(host), path, sizeof(path));
    assert(port2 == 443);
    assert(strcmp(host, "secure.eteros.org") == 0);
    assert(strcmp(path, "/latest.bin") == 0);

    /* Test 3: No Path */
    uint16_t port3 = parse_url("http://localhost", host, sizeof(host), path, sizeof(path));
    assert(port3 == 80);
    assert(strcmp(host, "localhost") == 0);
    assert(strcmp(path, "/") == 0);

    printf("All test_ota (parse_url) tests passed!\n");
    return 0;
}
