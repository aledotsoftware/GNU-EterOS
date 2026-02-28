#include <string.h>
#include <serial.h>
#include <vga.h>
#include <hal.h>
#include <task.h>
#include <net/socket.h>
#include <net/defs.h>

/**
 * éterOS - wget using Native Socket API
 */

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

void wget_run(const char* url_in) {
    char host[256];
    char path[256];
    uint16_t port = parse_url(url_in, host, sizeof(host), path, sizeof(path));
    
    terminal_write_string("[WGET] Target: ");
    if (port == 443) terminal_write_string("https://"); else terminal_write_string("http://");
    terminal_write_string(host);
    terminal_write_string("\n");

    if (port == 443) {
        terminal_write_string("[WGET] Error: TLS (HTTPS) no implementado. Conexión rechazada por seguridad.\n");
        return;
    }
    
    /* Use safe ip_aton from kernel/net/ip_utils.c (via net/defs.h) */
    uint32_t ip = ip_aton(host);
    if (ip == 0) {
        /* Simple hardcoded resolution for testing if not an IP */
        if (strcmp(host, "google.com") == 0) ip = 0x4850fa8e; /* 142.250.80.72 */
        else if (strcmp(host, "tudexgames.com") == 0) ip = 0x288C43AC; /* 172.67.140.40 */
        else {
            terminal_write_string("[WGET] Error: Solo soportamos IP o hosts harcodeados.\n");
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
    addr.sin_port = htons(port);
    addr.sin_addr = ip;
    
    terminal_write_string("[WGET] Connecting...\n");
    if (net_connect(sock, &addr, sizeof(addr)) != 0) {
        terminal_write_string("[WGET] Connection failed.\n");
        net_close(sock);
        return;
    }
    
    /* Increased buffer size to 1024 and added truncation checks */
    char request[1024];
    size_t req_size = sizeof(request);

    if (strlcpy(request, "GET ", req_size) >= req_size) goto trunc;
    if (strlcat(request, path, req_size) >= req_size) goto trunc;
    if (strlcat(request, " HTTP/1.0\r\nHost: ", req_size) >= req_size) goto trunc;
    if (strlcat(request, host, req_size) >= req_size) goto trunc;
    if (strlcat(request, "\r\nUser-Agent: eterOS/0.1\r\nConnection: close\r\n\r\n", req_size) >= req_size) goto trunc;
    
    net_send(sock, request, strlen(request), 0);
    goto receive;

trunc:
    terminal_write_string("[WGET] Error: Request buffer overflow or URL too long.\n");
    net_close(sock);
    return;

receive:
    
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
