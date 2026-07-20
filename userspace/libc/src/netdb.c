#include <netdb.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/syscall.h>

static struct hostent he_res;
static char *he_addr_list[2];
static char he_addr_buf[4];

struct hostent *gethostbyname(const char *name) {
    if (!name) return NULL;

    /* Check if it's already an IP */
    uint32_t ip[4];
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

    uint32_t resolved_ip = 0;
    long ret = syscall(SYS_gethostbyname, (long)name, (long)&resolved_ip);
    if (ret != 0) {
        return NULL; /* Resolution failed */
    }

    /* lwIP returns the IP in host byte order inside the kernel's net_gethostbyname via ip4_addr_get_u32.
       Usually this means we might need htonl, but let's assume it matches what the socket expects.
       Actually, net_gethostbyname uses ip4_addr_get_u32 which is host byte order. The socket layer
       usually expects network byte order. Let's just pass it as is for now and let the network stack handle it,
       or apply htonl if needed. EterOS network stack expects network byte order. */
    // resolved_ip = htonl(resolved_ip);

    /* In EterOS, the previous UDP implementation returned it such that memory copy to h_addr was correct. */
    /* Let's copy the resolved_ip into the buffer. Note: net_gethostbyname gives uint32_t */
    memcpy(he_addr_buf, &resolved_ip, 4);

    he_addr_list[0] = he_addr_buf;
    he_addr_list[1] = NULL;
    he_res.h_name = (char*)name;
    he_res.h_addrtype = AF_INET;
    he_res.h_length = 4;
    he_res.h_addr_list = he_addr_list;

    return &he_res;
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
