#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#ifdef assert
#undef assert
#endif
#define assert(x) do { if (!(x)) { printf("Assertion failed: %s\n", #x); exit(1); } } while (0)

#define OTA_MAX_URL 256

size_t strlcpy(char *dst, const char *src, size_t siz);

// Mock implementation for the test
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
    uint16_t port;

    // Test HTTP URL
    port = parse_url("http://repo.eteros.org/updates", host, sizeof(host), path, sizeof(path));
    assert(port == 80);
    assert(strcmp(host, "repo.eteros.org") == 0);
    assert(strcmp(path, "/updates") == 0);

    // Test HTTPS URL
    port = parse_url("https://secure.eteros.org/files", host, sizeof(host), path, sizeof(path));
    assert(port == 443);
    assert(strcmp(host, "secure.eteros.org") == 0);
    assert(strcmp(path, "/files") == 0);

    // Test No Protocol (Defaults to 80)
    port = parse_url("localhost/test", host, sizeof(host), path, sizeof(path));
    assert(port == 80);
    assert(strcmp(host, "localhost") == 0);
    assert(strcmp(path, "/test") == 0);

    // Test No Path
    port = parse_url("http://repo.eteros.org", host, sizeof(host), path, sizeof(path));
    assert(port == 80);
    assert(strcmp(host, "repo.eteros.org") == 0);
    assert(strcmp(path, "/") == 0);

    printf("All OTA URL parsing tests passed!\n");
    return 0;
}
