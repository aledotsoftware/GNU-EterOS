#include <string.h>
#include <serial.h>
#include <vga.h>
#include <hal.h>
#include <task.h>
#include <net/socket.h>

/**
 * éterOS - wget using Native Socket API
 */

static void parse_url(const char* url, char* host, size_t host_size, char* path, size_t path_size) {
    if (strncmp(url, "http://", 7) == 0) {
        url += 7;
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
    return 0;
}

static uint32_t ip_aton(const char* cp) {
    uint32_t val = 0;
    for (int i = 0; i < 4; i++) {
        uint32_t part = 0;
        while (*cp >= '0' && *cp <= '9') {
            part = part * 10 + (*cp - '0');
            cp++;
        }
        val |= (part << (i * 8));
        if (*cp == '.') cp++;
    }
    return val;
}

void wget_run(const char* url_in) {
    char host[256];
    char path[256];
    parse_url(url_in, host, sizeof(host), path, sizeof(path));
    
    terminal_write_string("[WGET] Host: ");
    terminal_write_string(host);
    terminal_write_string("\n");
    
    uint32_t ip = ip_aton(host);
    if (ip == 0) {
        /* Simple hardcoded resolution for testing if not an IP */
        if (strcmp(host, "google.com") == 0) ip = 0x4850fa8e; /* 142.250.80.72 */
        else if (strcmp(host, "tudexgames.com") == 0) ip = 0x288C43AC; /* 172.67.140.40 */
        else {
            terminal_write_string("[WGET] Error: Only IP addresses or hardcoded hosts supported currently.\n");
            return;
        }
    }
    
    socket_t sock = net_socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        terminal_write_string("[WGET] Failed to create socket.\n");
        return;
    }
    
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = 80;
    addr.sin_addr = ip;
    
    terminal_write_string("[WGET] Connecting...\n");
    if (net_connect(sock, &addr, sizeof(addr)) != 0) {
        terminal_write_string("[WGET] Connection failed.\n");
        net_close(sock);
        return;
    }
    
    char request[512];
    strlcpy(request, "GET ", sizeof(request));
    strlcat(request, path, sizeof(request));
    strlcat(request, " HTTP/1.0\r\nHost: ", sizeof(request));
    strlcat(request, host, sizeof(request));
    strlcat(request, "\r\nUser-Agent: eterOS/0.1\r\nConnection: close\r\n\r\n", sizeof(request));
    
    net_send(sock, request, strlen(request), 0);
    
    terminal_write_string("[WGET] Finalizado el envio de cabeceras. Recibiendo datos...\n\n");
    
    char buffer[1024];
    int len;
    while ((len = net_recv(sock, buffer, sizeof(buffer) - 1, 0)) > 0) {
        buffer[len] = '\0';
        for (int i = 0; i < len; i++) {
            terminal_putchar(buffer[i]);
        }
        task_yield();
    }
    
    terminal_write_string("\n[WGET] Done.\n");
    net_close(sock);
}
