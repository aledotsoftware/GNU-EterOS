#include <stdio.h>
#include <string.h>
#include <assert.h>

#ifndef __ETEROS_HOST_TEST__
#define __ETEROS_HOST_TEST__
#endif

#include "net/socket.h"

/* Mock dependencies from terminal, etc. */
void terminal_write_string(const char* str) {}
void terminal_putchar(char c) {}
void task_yield(void) {}

int net_connect(socket_t sock, const struct sockaddr_in_old* addr, int addrlen) { return -1; }
int net_send(socket_t sock, const void* buf, int len, int flags) { return -1; }
int net_recv(socket_t sock, void* buf, int len, int flags) { return -1; }
int net_close(socket_t sock) { return 0; }
uint32_t ip_aton(const char *cp) { return 0; }
socket_t net_socket(int domain, int type, int protocol) { return -1; }

#define parse_url __wget_parse_url
#include "../kernel/apps/wget.c"
#undef parse_url

#define parse_url __wget_parse_url

/* Ensure standard assert works since it gets overriden sometimes */
#undef assert
#define assert(x) if (!(x)) { printf("Assertion failed: %s\n", #x); return 1; }

int main() {
    char host[256];
    char path[256];
    uint16_t port;

    printf("Running wget parse_url tests...\n");

    /* Test 1: HTTP URL with path */
    port = parse_url("http://example.com/index.html", host, sizeof(host), path, sizeof(path));
    assert(port == 80);
    assert(strcmp(host, "example.com") == 0);
    assert(strcmp(path, "/index.html") == 0);

    /* Test 2: HTTPS URL with path */
    port = parse_url("https://example.com/index.html", host, sizeof(host), path, sizeof(path));
    assert(port == 443);
    assert(strcmp(host, "example.com") == 0);
    assert(strcmp(path, "/index.html") == 0);

    /* Test 3: URL without path (should default to "/") */
    port = parse_url("http://example.com", host, sizeof(host), path, sizeof(path));
    assert(port == 80);
    assert(strcmp(host, "example.com") == 0);
    assert(strcmp(path, "/") == 0);

    /* Test 4: URL without protocol */
    port = parse_url("example.com/test", host, sizeof(host), path, sizeof(path));
    assert(port == 80);
    assert(strcmp(host, "example.com") == 0);
    assert(strcmp(path, "/test") == 0);

    /* Test 5: Host truncation */
    char small_host[5];
    port = parse_url("http://example.com/index.html", small_host, sizeof(small_host), path, sizeof(path));
    assert(port == 80);
    assert(strcmp(small_host, "exam") == 0);
    assert(strcmp(path, "/index.html") == 0);

    /* Test 6: Path truncation */
    char small_path[6];
    port = parse_url("http://example.com/index.html", host, sizeof(host), small_path, sizeof(small_path));
    assert(port == 80);
    assert(strcmp(host, "example.com") == 0);
    assert(strcmp(small_path, "/inde") == 0);

    printf("All parse_url tests passed!\n");
    return 0;
}
