#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>

/* Basic DNS Header */
struct dns_header {
    uint16_t id;
    uint16_t flags;
    uint16_t qdcount;
    uint16_t ancount;
    uint16_t nscount;
    uint16_t arcount;
} __attribute__((packed));

static uint32_t get_dns_server() {
    /* Try to read /etc/resolv.conf */
    int fd = open("/etc/resolv.conf", O_RDONLY);
    if (fd >= 0) {
        char buf[256];
        int n = read(fd, buf, sizeof(buf)-1);
        close(fd);
        if (n > 0) {
            buf[n] = 0;
            char *p = strstr(buf, "nameserver ");
            if (p) {
                p += 11;
                uint32_t ip[4] = {0};
                /* Simple IPv4 parse */
                const char *s = p;
                for (int i=0; i<4; i++) {
                    while (*s == ' ' || *s == '\t') s++;
                    int val = 0;
                    while (*s >= '0' && *s <= '9') {
                        val = val * 10 + (*s - '0');
                        s++;
                    }
                    ip[i] = val;
                    if (*s == '.') s++;
                }
                uint32_t addr = (ip[0]) | (ip[1] << 8) | (ip[2] << 16) | (ip[3] << 24);
                return addr;
            }
        }
    }
    /* Default to 8.8.8.8 */
    return (8) | (8 << 8) | (8 << 16) | (8 << 24);
}

static void encode_domain(char *dst, const char *src) {
    char *p = dst;
    const char *s = src;
    while (*s) {
        const char *dot = strchr(s, '.');
        if (!dot) dot = s + strlen(s);
        int len = dot - s;
        *p++ = len;
        memcpy(p, s, len);
        p += len;
        s = dot;
        if (*s == '.') s++;
    }
    *p = 0;
}

static struct hostent he_res;
static char *he_addr_list[2];
static char he_addr_buf[4];

struct hostent *gethostbyname(const char *name) {
    /* Check if it's already an IP */
    uint32_t ip[4];
    int parsed __attribute__((unused)) = 0;
    const char *s = name;
    for(int i=0; i<4; i++){
        int v = 0;
        while(*s>='0' && *s<='9') v = v*10 + (*s++ - '0');
        ip[i] = v;
        if(*s == '.') s++;
    }
    if(!*s) {
        uint32_t addr = (ip[0]) | (ip[1] << 8) | (ip[2] << 16) | (ip[3] << 24);
        memcpy(he_addr_buf, &addr, 4);
        he_addr_list[0] = he_addr_buf;
        he_addr_list[1] = NULL;
        he_res.h_name = (char*)name;
        he_res.h_addrtype = AF_INET;
        he_res.h_length = 4;
        he_res.h_addr_list = he_addr_list;
        return &he_res;
    }

    uint32_t dns_ip = get_dns_server();

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) return NULL;

    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(53);
    dest.sin_addr = dns_ip;

    uint8_t packet[512];
    memset(packet, 0, sizeof(packet));

    struct dns_header *hdr = (struct dns_header*)packet;
    hdr->id = htons(1234);
    hdr->flags = htons(0x0100); /* Standard query */
    hdr->qdcount = htons(1);

    char *qname = (char*)(packet + sizeof(struct dns_header));
    encode_domain(qname, name);
    int qname_len = strlen(qname) + 1;

    uint16_t *qtype = (uint16_t*)(qname + qname_len);
    *qtype = htons(1); /* A record */
    uint16_t *qclass = (uint16_t*)(qname + qname_len + 2);
    *qclass = htons(1); /* IN */

    int req_len = sizeof(struct dns_header) + qname_len + 4;
    sendto(sock, packet, req_len, 0, (struct sockaddr*)&dest, sizeof(dest));

    struct sockaddr_in src;
    socklen_t srclen = sizeof(src);
    int res = recvfrom(sock, packet, sizeof(packet), 0, (struct sockaddr*)&src, &srclen);
    close(sock);

    if (res < (int)sizeof(struct dns_header)) return NULL;

    uint16_t flags = ntohs(hdr->flags);
    if ((flags & 0x000F) != 0) return NULL; /* Error code */

    int ancount = ntohs(hdr->ancount);
    if (ancount == 0) return NULL;

    /* Parse answer (skip question) */
    uint8_t *p = packet + sizeof(struct dns_header);
    while (*p) {
        if ((*p & 0xC0) == 0xC0) {
            p += 2;
            break;
        } else {
            p += *p + 1;
        }
    }
    if ((*p & 0xC0) != 0xC0) p++;
    p += 4; /* qtype, qclass */

    /* Read first answer */
    for (int i = 0; i < ancount; i++) {
        if (p >= packet + res) break;
        if ((*p & 0xC0) == 0xC0) p += 2; /* compressed name */
        else {
            while (*p && p < packet + res) p += *p + 1;
            p++;
        }
        uint16_t type = ntohs(*(uint16_t*)p); p += 2;
        uint16_t class = ntohs(*(uint16_t*)p); p += 2;
        p += 4; /* ttl */
        uint16_t rdlength = ntohs(*(uint16_t*)p); p += 2;

        if (type == 1 && class == 1 && rdlength == 4) {
            memcpy(he_addr_buf, p, 4);
            he_addr_list[0] = he_addr_buf;
            he_addr_list[1] = NULL;
            he_res.h_name = (char*)name;
            he_res.h_addrtype = AF_INET;
            he_res.h_length = 4;
            he_res.h_addr_list = he_addr_list;
            return &he_res;
        }
        p += rdlength;
    }

    return NULL;
}

int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res) {
    (void)hints;
    struct hostent *he = gethostbyname(node);
    if (!he) return EAI_NONAME;

    struct addrinfo *ai = malloc(sizeof(struct addrinfo));
    if (!ai) return EAI_MEMORY;
    memset(ai, 0, sizeof(struct addrinfo));

    struct sockaddr_in *sa = malloc(sizeof(struct sockaddr_in));
    if (!sa) { free(ai); return EAI_MEMORY; }
    memset(sa, 0, sizeof(struct sockaddr_in));

    sa->sin_family = AF_INET;
    if (service) {
        sa->sin_port = htons(atoi(service));
    }
    memcpy(&sa->sin_addr, he->h_addr, 4);

    ai->ai_family = AF_INET;
    ai->ai_socktype = hints ? hints->ai_socktype : SOCK_STREAM;
    ai->ai_protocol = hints ? hints->ai_protocol : IPPROTO_TCP;
    ai->ai_addrlen = sizeof(struct sockaddr_in);
    ai->ai_addr = (struct sockaddr*)sa;

    *res = ai;
    return 0;
}

void freeaddrinfo(struct addrinfo *res) {
    if (res) {
        if (res->ai_addr) free(res->ai_addr);
        if (res->ai_canonname) free(res->ai_canonname);
        free(res);
    }
}
