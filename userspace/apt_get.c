#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netdb.h>
#include <errno.h>

#define AR_MAGIC "!<arch>\n"
#define AR_HEADER_SIZE 60
#define TAR_BLOCK_SIZE 512
#define COPY_BUFFER_SIZE 4096
#define HTTP_BUFFER_SIZE 2048
#define HTTP_HEADER_LIMIT 8192

typedef enum {
    PACKAGE_CURATED_DATA,
    PACKAGE_BLOCKED_RUNTIME
} package_kind_t;

typedef struct {
    const char *name;
    const char *url;
    const char *description;
    package_kind_t kind;
    const char *notes;
} package_entry_t;

static const package_entry_t g_packages[] = {
    {
        "fonts-dejavu-core",
        "http://ftp.us.debian.org/debian/pool/main/f/fonts-dejavu/fonts-dejavu-core_2.37-8_all.deb",
        "Fuentes TrueType utiles para UI y render de texto",
        PACKAGE_CURATED_DATA,
        "Paquete de datos: buen candidato para EterOS, pero Debian hoy lo publica comprimido."
    },
    {
        "hicolor-icon-theme",
        "http://ftp.us.debian.org/debian/pool/main/h/hicolor-icon-theme/hicolor-icon-theme_0.18-2_all.deb",
        "Tema base de iconos Freedesktop",
        PACKAGE_CURATED_DATA,
        "Paquete de datos: aporta assets graficos, sin depender de ejecutar binarios Linux."
    },
    {
        "iso-codes",
        "http://ftp.us.debian.org/debian/pool/main/i/iso-codes/iso-codes_4.18.0-1_all.deb",
        "Tablas ISO de idiomas, paises y monedas",
        PACKAGE_CURATED_DATA,
        "Paquete de datos: util para localizacion y configuracion."
    },
    {
        "nano",
        "http://ftp.us.debian.org/debian/pool/main/n/nano/nano_8.4-1_amd64.deb",
        "Editor de texto GNU nano",
        PACKAGE_BLOCKED_RUNTIME,
        "Bloqueado por ahora: el binario Debian depende de libc6 y ncurses/tinfo de Linux."
    }
};

static const package_entry_t *find_package(const char *name) {
    for (size_t i = 0; i < sizeof(g_packages) / sizeof(g_packages[0]); ++i) {
        if (strcmp(g_packages[i].name, name) == 0) {
            return &g_packages[i];
        }
    }
    return NULL;
}

static int starts_with(const char *s, const char *prefix) {
    if (!s || !prefix) return 0;
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

static int is_deb_path(const char *arg) {
    size_t len;

    if (!arg) return 0;
    len = strlen(arg);
    if (len < 4) return 0;
    return strcmp(arg + len - 4, ".deb") == 0;
}

static int is_http_url(const char *arg) {
    return starts_with(arg, "http://");
}

static int parse_decimal_field(const char *buf, size_t size) {
    char tmp[32];
    size_t i = 0;

    while (i < size && i < sizeof(tmp) - 1) {
        if (buf[i] == ' ' || buf[i] == '\0') break;
        tmp[i] = buf[i];
        i++;
    }
    tmp[i] = '\0';
    return atoi(tmp);
}

static unsigned long parse_octal_field(const char *buf, size_t size) {
    unsigned long value = 0;

    for (size_t i = 0; i < size; ++i) {
        char c = buf[i];
        if (c == '\0' || c == ' ') break;
        if (c < '0' || c > '7') continue;
        value = (value << 3) + (unsigned long)(c - '0');
    }

    return value;
}

static void normalize_tar_path(const char *in, char *out, size_t out_size) {
    size_t start = 0;

    out[0] = '\0';
    if (!in || !in[0]) return;

    while (in[start] == '.' || in[start] == '/') {
        if (in[start] == '.' && in[start + 1] == '/') {
            start += 2;
            continue;
        }
        if (in[start] == '/') {
            start += 1;
            continue;
        }
        break;
    }

    strlcpy(out, "/", out_size);
    strlcat(out, in + start, out_size);
}

static int mkdir_p(const char *path) {
    char tmp[256];
    size_t len;

    if (!path || !path[0]) return 0;
    strlcpy(tmp, path, sizeof(tmp));
    len = strlen(tmp);
    if (len == 0) return 0;

    for (size_t i = 1; i < len; ++i) {
        if (tmp[i] == '/') {
            tmp[i] = '\0';
            mkdir(tmp, 0755);
            tmp[i] = '/';
        }
    }

    mkdir(tmp, 0755);
    return 0;
}

static void ensure_parent_dirs(const char *path) {
    char tmp[256];
    char *slash;

    strlcpy(tmp, path, sizeof(tmp));
    slash = strrchr(tmp, '/');
    if (!slash || slash == tmp) {
        mkdir("/", 0755);
        return;
    }

    *slash = '\0';
    mkdir_p(tmp);
}

static int skip_bytes(FILE *fp, unsigned long count) {
    char buffer[256];

    while (count > 0) {
        size_t chunk = count > sizeof(buffer) ? sizeof(buffer) : (size_t)count;
        if (fread(buffer, 1, chunk, fp) != chunk) {
            return 1;
        }
        count -= (unsigned long)chunk;
    }
    return 0;
}

static int copy_regular_file(FILE *src, unsigned long size, const char *dst_path) {
    char buffer[COPY_BUFFER_SIZE];
    FILE *dst;
    unsigned long remaining = size;

    ensure_parent_dirs(dst_path);
    dst = fopen(dst_path, "wb");
    if (!dst) {
        printf("apt-get: no se pudo crear %s\n", dst_path);
        return 1;
    }

    while (remaining > 0) {
        size_t chunk = remaining > sizeof(buffer) ? sizeof(buffer) : (size_t)remaining;
        if (fread(buffer, 1, chunk, src) != chunk) {
            fclose(dst);
            printf("apt-get: fallo al leer contenido de tar\n");
            return 1;
        }
        if (fwrite(buffer, 1, chunk, dst) != chunk) {
            fclose(dst);
            printf("apt-get: fallo al escribir %s\n", dst_path);
            return 1;
        }
        remaining -= (unsigned long)chunk;
    }

    fclose(dst);
    return 0;
}

static int extract_tar_member(FILE *deb, long tar_offset, long tar_size) {
    unsigned char header[TAR_BLOCK_SIZE];
    long consumed = 0;

    if (fseek(deb, tar_offset, SEEK_SET) != 0) {
        return 1;
    }

    while (consumed + TAR_BLOCK_SIZE <= tar_size) {
        char name[156];
        char prefix[156];
        char full_name[256];
        char out_path[256];
        unsigned long file_size;
        char typeflag;
        unsigned long padded_size;
        int header_is_zero = 1;

        if (fread(header, 1, TAR_BLOCK_SIZE, deb) != TAR_BLOCK_SIZE) {
            return 1;
        }
        consumed += TAR_BLOCK_SIZE;

        for (int i = 0; i < TAR_BLOCK_SIZE; ++i) {
            if (header[i] != 0) {
                header_is_zero = 0;
                break;
            }
        }
        if (header_is_zero) {
            break;
        }

        memcpy(name, header, 100);
        name[100] = '\0';
        memcpy(prefix, header + 345, 155);
        prefix[155] = '\0';

        full_name[0] = '\0';
        if (prefix[0]) {
            strlcpy(full_name, prefix, sizeof(full_name));
            strlcat(full_name, "/", sizeof(full_name));
        }
        strlcat(full_name, name, sizeof(full_name));

        normalize_tar_path(full_name, out_path, sizeof(out_path));
        file_size = parse_octal_field((const char *)(header + 124), 12);
        typeflag = (char)header[156];
        padded_size = ((file_size + TAR_BLOCK_SIZE - 1) / TAR_BLOCK_SIZE) * TAR_BLOCK_SIZE;

        if (typeflag == '5') {
            mkdir_p(out_path);
            if (skip_bytes(deb, padded_size) != 0) return 1;
        } else if (typeflag == '0' || typeflag == '\0') {
            if (copy_regular_file(deb, file_size, out_path) != 0) return 1;
            if (padded_size > file_size && skip_bytes(deb, padded_size - file_size) != 0) return 1;
        } else {
            if (skip_bytes(deb, padded_size) != 0) return 1;
        }

        consumed += (long)padded_size;
    }

    return 0;
}

static int parse_http_status(const char *header_buf) {
    const char *p = strchr(header_buf, ' ');
    if (!p) return 0;
    while (*p == ' ') p++;
    return atoi(p);
}

static const char *find_header_terminator(const char *buf, size_t len) {
    for (size_t i = 0; i + 3 < len; ++i) {
        if (buf[i] == '\r' && buf[i + 1] == '\n' && buf[i + 2] == '\r' && buf[i + 3] == '\n') {
            return buf + i;
        }
    }
    return NULL;
}

static int parse_url(const char *url, char *host, size_t host_size, char *path, size_t path_size, char *port, size_t port_size) {
    const char *cursor;
    const char *slash;
    const char *colon;
    size_t host_len;

    if (!starts_with(url, "http://")) {
        printf("E: solo se soporta http:// por ahora\n");
        return 1;
    }

    cursor = url + 7;
    slash = strchr(cursor, '/');
    colon = strchr(cursor, ':');

    if (!slash) {
        slash = cursor + strlen(cursor);
    }

    if (colon && colon < slash) {
        host_len = (size_t)(colon - cursor);
        strlcpy(port, colon + 1, port_size);
        if (slash != cursor + strlen(cursor)) {
            port[slash - colon - 1] = '\0';
        }
    } else {
        host_len = (size_t)(slash - cursor);
        strlcpy(port, "80", port_size);
    }

    if (host_len == 0 || host_len >= host_size) {
        printf("E: host invalido en URL: %s\n", url);
        return 1;
    }

    memcpy(host, cursor, host_len);
    host[host_len] = '\0';

    if (*slash) {
        strlcpy(path, slash, path_size);
    } else {
        strlcpy(path, "/", path_size);
    }

    return 0;
}

static void filename_from_url(const char *url, char *out, size_t out_size) {
    const char *slash = strrchr(url, '/');

    if (!slash || !slash[1]) {
        strlcpy(out, "download.deb", out_size);
        return;
    }

    strlcpy(out, slash + 1, out_size);
}

static void build_cache_path_for_url(const char *url, char *out, size_t out_size) {
    char filename[128];

    filename_from_url(url, filename, sizeof(filename));
    strlcpy(out, "/var/cache/apt/archives/", out_size);
    strlcat(out, filename, out_size);
}

static int http_download_to_file(const char *url, const char *dst_path) {
    char host[256];
    char path[512];
    char port[16];
    char request[1024];
    char recv_buf[HTTP_BUFFER_SIZE];
    char header_buf[HTTP_HEADER_LIMIT];
    size_t header_used = 0;
    int header_done = 0;
    int status_code = 0;
    FILE *out = NULL;
    struct addrinfo hints;
    struct addrinfo *res = NULL;
    int sock = -1;
    int rc = 1;

    if (parse_url(url, host, sizeof(host), path, sizeof(path), port, sizeof(port)) != 0) {
        return 1;
    }

    ensure_parent_dirs(dst_path);
    out = fopen(dst_path, "wb");
    if (!out) {
        printf("E: no se pudo crear %s\n", dst_path);
        return 1;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    if (getaddrinfo(host, port, &hints, &res) != 0 || !res) {
        printf("E: no se pudo resolver %s\n", host);
        goto cleanup;
    }

    sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (sock < 0) {
        printf("E: no se pudo crear socket\n");
        goto cleanup;
    }

    if (connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
        printf("E: no se pudo conectar a %s:%s\n", host, port);
        goto cleanup;
    }

    request[0] = '\0';
    strlcpy(request, "GET ", sizeof(request));
    strlcat(request, path, sizeof(request));
    strlcat(request, " HTTP/1.0\r\nHost: ", sizeof(request));
    strlcat(request, host, sizeof(request));
    strlcat(request, "\r\nUser-Agent: eterOS-apt/0.2\r\nConnection: close\r\n\r\n", sizeof(request));

    if (send(sock, request, strlen(request), 0) < 0) {
        printf("E: no se pudo enviar request HTTP\n");
        goto cleanup;
    }

    while (1) {
        ssize_t len = recv(sock, recv_buf, sizeof(recv_buf), 0);
        if (len < 0) {
            printf("E: fallo de recepcion HTTP\n");
            goto cleanup;
        }
        if (len == 0) {
            break;
        }

        if (!header_done) {
            size_t copy_len = (size_t)len;
            if (header_used + copy_len >= sizeof(header_buf)) {
                printf("E: cabecera HTTP demasiado grande\n");
                goto cleanup;
            }

            memcpy(header_buf + header_used, recv_buf, copy_len);
            header_used += copy_len;
            header_buf[header_used] = '\0';

            const char *hdr_end = find_header_terminator(header_buf, header_used);
            if (!hdr_end) {
                continue;
            }

            status_code = parse_http_status(header_buf);
            header_done = 1;

            if (status_code != 200) {
                printf("E: HTTP %d descargando %s\n", status_code, url);
                goto cleanup;
            }

            hdr_end += 4;
            {
                size_t body_offset = (size_t)(hdr_end - header_buf);
                size_t body_len = header_used - body_offset;
                if (body_len > 0 && fwrite(header_buf + body_offset, 1, body_len, out) != body_len) {
                    printf("E: no se pudo escribir %s\n", dst_path);
                    goto cleanup;
                }
            }
        } else {
            if (fwrite(recv_buf, 1, (size_t)len, out) != (size_t)len) {
                printf("E: no se pudo escribir %s\n", dst_path);
                goto cleanup;
            }
        }
    }

    rc = 0;

cleanup:
    if (sock >= 0) close(sock);
    if (res) freeaddrinfo(res);
    if (out) fclose(out);
    if (rc != 0) {
        unlink(dst_path);
    }
    return rc;
}

static int fetch_deb_from_url(const char *url, char *cache_path, size_t cache_path_size) {
    build_cache_path_for_url(url, cache_path, cache_path_size);
    printf("Descargando %s\n", url);
    printf("Guardando en %s\n", cache_path);

    if (http_download_to_file(url, cache_path) != 0) {
        return 1;
    }

    printf("Descarga completa: %s\n", cache_path);
    return 0;
}

static int install_deb_file(const char *deb_path) {
    FILE *fp;
    char global_magic[8];
    int found_data = 0;

    fp = fopen(deb_path, "rb");
    if (!fp) {
        printf("E: no se pudo abrir %s\n", deb_path);
        return 1;
    }

    if (fread(global_magic, 1, sizeof(global_magic), fp) != sizeof(global_magic) ||
        memcmp(global_magic, AR_MAGIC, sizeof(global_magic)) != 0) {
        fclose(fp);
        printf("E: archivo .deb invalido: %s\n", deb_path);
        return 1;
    }

    while (1) {
        char header[AR_HEADER_SIZE];
        char name[17];
        int file_size;
        long data_offset;

        if (fread(header, 1, sizeof(header), fp) == 0) {
            break;
        }
        if (memcmp(header + 58, "`\n", 2) != 0) {
            fclose(fp);
            printf("E: cabecera ar invalida en %s\n", deb_path);
            return 1;
        }

        memcpy(name, header, 16);
        name[16] = '\0';
        for (int i = 15; i >= 0; --i) {
            if (name[i] == ' ' || name[i] == '/') {
                name[i] = '\0';
            } else {
                break;
            }
        }

        file_size = parse_decimal_field(header + 48, 10);
        data_offset = ftell(fp);

        if (strcmp(name, "data.tar") == 0) {
            if (extract_tar_member(fp, data_offset, file_size) != 0) {
                fclose(fp);
                printf("E: fallo al extraer data.tar de %s\n", deb_path);
                return 1;
            }
            found_data = 1;
            break;
        }

        if (strcmp(name, "data.tar.gz") == 0 || strcmp(name, "data.tar.xz") == 0 || strcmp(name, "data.tar.zst") == 0) {
            fclose(fp);
            printf("E: %s usa %s y eterOS todavia no soporta esa compresion dentro del .deb\n", deb_path, name);
            return 1;
        }

        if (fseek(fp, data_offset + file_size + (file_size & 1), SEEK_SET) != 0) {
            fclose(fp);
            printf("E: fallo al avanzar en %s\n", deb_path);
            return 1;
        }
    }

    fclose(fp);

    if (!found_data) {
        printf("E: .deb sin data.tar soportado: %s\n", deb_path);
        return 1;
    }

    return 0;
}

static void print_usage(void) {
    printf("Uso: apt-get <comando> [paquetes|archivos.deb|urls]\n");
    printf("Comandos:\n");
    printf("  update\n");
    printf("  list\n");
    printf("  fetch <pkg|http://archivo.deb>\n");
    printf("  install <pkg|./archivo.deb|http://archivo.deb>\n");
    printf("\n");
    printf("Notas:\n");
    printf("  - apt-get ya no instala paquetes ficticios del initrd\n");
    printf("  - descarga .deb reales via HTTP a /var/cache/apt/archives\n");
    printf("  - solo extrae .deb con data.tar sin compresion\n");
    printf("  - HTTPS, data.tar.xz y binarios Debian dinamicos siguen pendientes\n");
}

static const char *package_kind_label(package_kind_t kind) {
    switch (kind) {
        case PACKAGE_CURATED_DATA: return "curado/datos";
        case PACKAGE_BLOCKED_RUNTIME: return "bloqueado/runtime";
        default: return "desconocido";
    }
}

static void print_package_list(void) {
    for (size_t i = 0; i < sizeof(g_packages) / sizeof(g_packages[0]); ++i) {
        printf("%s [%s]\n", g_packages[i].name, package_kind_label(g_packages[i].kind));
        printf("  %s\n", g_packages[i].description);
        printf("  %s\n", g_packages[i].notes);
        printf("  %s\n", g_packages[i].url);
    }
}

static int fetch_from_argument(const char *arg, char *cache_path, size_t cache_path_size, const package_entry_t **pkg_out) {
    const package_entry_t *pkg = NULL;
    const char *url = NULL;

    if (pkg_out) *pkg_out = NULL;

    if (is_http_url(arg)) {
        url = arg;
    } else {
        pkg = find_package(arg);
        if (!pkg) {
            printf("E: No se encontro el paquete %s\n", arg);
            return 1;
        }
        url = pkg->url;
    }

    if (pkg_out) *pkg_out = pkg;
    return fetch_deb_from_url(url, cache_path, cache_path_size);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    if (strcmp(argv[1], "update") == 0) {
        printf("Leyendo listas de paquetes... Hecho\n");
        printf("Catalogo curado remoto: Debian pool via HTTP\n");
        printf("Cache local: /var/cache/apt/archives\n");
        return 0;
    }

    if (strcmp(argv[1], "list") == 0) {
        print_package_list();
        return 0;
    }

    if (strcmp(argv[1], "fetch") == 0) {
        int rc = 0;

        if (argc < 3) {
            printf("apt-get: faltan paquetes para descargar\n");
            return 1;
        }

        for (int i = 2; i < argc; ++i) {
            char cache_path[256];
            if (fetch_from_argument(argv[i], cache_path, sizeof(cache_path), NULL) != 0) {
                rc = 1;
            }
        }

        return rc;
    }

    if (strcmp(argv[1], "install") == 0) {
        int rc = 0;

        if (argc < 3) {
            printf("apt-get: faltan paquetes para instalar\n");
            return 1;
        }

        for (int i = 2; i < argc; ++i) {
            const package_entry_t *pkg = NULL;
            const char *deb_path = NULL;
            char cache_path[256];

            if (is_deb_path(argv[i]) && !is_http_url(argv[i])) {
                deb_path = argv[i];
            } else if (is_http_url(argv[i])) {
                if (fetch_from_argument(argv[i], cache_path, sizeof(cache_path), &pkg) != 0) {
                    rc = 1;
                    continue;
                }
                deb_path = cache_path;
            } else {
                pkg = find_package(argv[i]);
                if (!pkg) {
                    printf("E: No se encontro el paquete %s\n", argv[i]);
                    rc = 1;
                    continue;
                }

                if (pkg->kind == PACKAGE_BLOCKED_RUNTIME) {
                    printf("E: %s esta bloqueado para instalacion automatica\n", pkg->name);
                    printf("   %s\n", pkg->notes);
                    rc = 1;
                    continue;
                }

                if (fetch_deb_from_url(pkg->url, cache_path, sizeof(cache_path)) != 0) {
                    rc = 1;
                    continue;
                }
                deb_path = cache_path;
            }

            if (install_deb_file(deb_path) != 0) {
                rc = 1;
            } else {
                printf("Instalado desde %s\n", deb_path);
            }
        }

        return rc;
    }

    printf("apt-get: comando no soportado: %s\n", argv[1]);
    print_usage();
    return 1;
}
