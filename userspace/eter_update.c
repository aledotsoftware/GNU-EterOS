#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>

extern uint16_t htons(uint16_t v);
extern uint32_t htonl(uint32_t v);

/* Helper to parse basic IPv4 from string (e.g. 10.0.2.2) */
static uint32_t parse_ip(const char *ip_str) {
    uint32_t ip = 0;
    int a, b, c, d;
    const char *p = ip_str;
    a = atoi(p); while (*p && *p != '.') p++; if (*p) p++;
    b = atoi(p); while (*p && *p != '.') p++; if (*p) p++;
    c = atoi(p); while (*p && *p != '.') p++; if (*p) p++;
    d = atoi(p);
    
    ip = (a << 24) | (b << 16) | (c << 8) | d;
    return htonl(ip);
}

static int fetch_metadata_path(uint32_t server_ip, const char *domain, char *out_path, size_t out_size) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("DEBUG: socket() failed\n");
        return -1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8000);
    addr.sin_addr = server_ip;

    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("DEBUG: connect() to 8000 failed\n");
        close(sock);
        return -1;
    }

    char req[256];
    snprintf(req, sizeof(req), "GET /metadata.json HTTP/1.1\r\nHost: %s\r\nConnection: close\r\n\r\n", domain);
    send(sock, req, strlen(req), 0);
    
    char buf[1024];
    int total = 0;
    while (total < (int)sizeof(buf) - 1) {
        int n = recv(sock, buf + total, sizeof(buf) - 1 - total, 0);
        if (n <= 0) break;
        total += n;
    }
    buf[total] = '\0';
    close(sock);

    if (total == 0) {
        printf("DEBUG: recv() read 0 bytes\n");
        return -1;
    }

    char *ptr = strstr(buf, "\"path\"");
    if (!ptr) {
        printf("DEBUG: strstr failed. BUF: %s\n", buf);
        return -1;
    }
    
    ptr += 6; // salta "path"
    
    // Saltamos espacios, tabs, dos puntos y comillas
    while (*ptr == ' ' || *ptr == '\t' || *ptr == ':' || *ptr == '"') {
        ptr++;
    }
    
    int i = 0;
    while (*ptr != '"' && *ptr != '\n' && *ptr != '\r' && *ptr != '\0' && i < (int)out_size - 1) {
        out_path[i++] = *ptr++;
    }
    out_path[i] = '\0';
    
    return 0;
}

int main(int argc, char *argv[]) {
    printf("[OTA] EterOS Over-The-Air Updater\n");

    const char *server_ip_str = "10.0.2.2";
    #define METADATA_HOST "10.0.2.2"
    const char *domain = "update.tudexnetworks.com";
    const char *version = NULL;

    if (argc >= 2) {
        // Option to specify version
        version = argv[1];
    }

    uint32_t server_ip = parse_ip(server_ip_str);
    
    char fetch_path[128];
    if (version) {
        snprintf(fetch_path, sizeof(fetch_path), "/%s/eteros.img", version);
    } else {
        printf("[OTA] Consultando metadata.json para buscar version mas reciente...\n");
        if (fetch_metadata_path(server_ip, domain, fetch_path, sizeof(fetch_path)) < 0) {
            printf("[OTA] Error: No se pudo obtener o parsear metadata.json.\n");
            return 1;
        }
        printf("[OTA] Version dinamica resuelta: %s\n", fetch_path);
    }

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        printf("[OTA] Error: No se pudo crear el socket.\n");
        return 1;
    }

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8000); // HTTP
    addr.sin_addr = server_ip;

    printf("[OTA] Conectando al servidor %s para descargar imagen...\n", server_ip_str);
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        printf("[OTA] Error: Conexion fallida.\n");
        close(sock);
        return 1;
    }

    printf("[OTA] Conexion establecida. Solicitando %s a %s...\n", fetch_path, domain);

    char request[256];
    snprintf(request, sizeof(request), 
             "GET %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "User-Agent: EterOS-OTA/1.0\r\n"
             "Connection: close\r\n\r\n", fetch_path, domain);

    send(sock, request, strlen(request), 0);

    // Skip HTTP Headers
    printf("[OTA] Recibiendo datos...\n");
    char buf[512];
    int headers_ended = 0;
    
    // Simplistic header parser (looks for \r\n\r\n)
    char header_buf[2048];
    int header_len = 0;
    
    while (!headers_ended && header_len < (int)sizeof(header_buf) - 1) {
        int n = recv(sock, header_buf + header_len, 1, 0);
        if (n <= 0) {
            printf("[OTA] Error leyendo cabeceras.\n");
            close(sock);
            return 1;
        }
        header_len++;
        header_buf[header_len] = '\0';
        
        if (header_len >= 4) {
            if (header_buf[header_len-4] == '\r' && header_buf[header_len-3] == '\n' &&
                header_buf[header_len-2] == '\r' && header_buf[header_len-1] == '\n') {
                headers_ended = 1;
            }
        }
    }
    
    if (!headers_ended) {
        printf("[OTA] Error: Cabeceras demasiado grandes.\n");
        close(sock);
        return 1;
    }

    printf("[OTA] Cabeceras HTTP recibidas. Escribiendo a disco (/dev/sda)...\n");

    int disk_fd = open("/dev/sda", O_WRONLY);
    if (disk_fd < 0) {
        printf("[OTA] Error: No se pudo abrir /dev/sda (Raw Disk).\n");
        close(sock);
        return 1;
    }

    // Now write the rest of the stream to the disk
    size_t total_written = 0;
    while (1) {
        int n = recv(sock, buf, sizeof(buf), 0);
        if (n <= 0) break; // Connection closed by server or error
        
        ssize_t w = write(disk_fd, buf, n);
        if (w < 0) {
            printf("\n[OTA] Error fatal escribiendo en el disco!\n");
            close(disk_fd);
            close(sock);
            return 1;
        }
        total_written += w;
        
        if ((total_written % (1024 * 1024)) == 0) {
            printf("\r[OTA] Descargados: %zu MB", total_written / (1024 * 1024));
        }
    }

    printf("\n[OTA] Descarga y flasheo completos. Total: %zu bytes.\n", total_written);
    close(disk_fd);
    close(sock);

    printf("[OTA] Actualizacion instalada en /dev/sda.\n");
    printf("[OTA] Reiniciando el sistema en 3 segundos...\n");
    
    for (volatile int i = 0; i < 50000000; i++); // simple delay

    printf("[OTA] Ejecutando syscall reboot...\n");
    reboot(0xCAFE, 0xBABE, 1, NULL);

    return 0;
}
