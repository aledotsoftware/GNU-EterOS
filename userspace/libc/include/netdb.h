#ifndef _NETDB_H
#define _NETDB_H

#include <stdint.h>
#include <sys/socket.h>

struct hostent {
    char  *h_name;            /* official name of host */
    char **h_aliases;         /* alias list */
    int    h_addrtype;        /* host address type */
    int    h_length;          /* length of address */
    char **h_addr_list;       /* list of addresses */
};

#define h_addr h_addr_list[0] /* for backward compatibility */

struct addrinfo {
    int              ai_flags;
    int              ai_family;
    int              ai_socktype;
    int              ai_protocol;
    socklen_t        ai_addrlen;
    struct sockaddr *ai_addr;
    char            *ai_canonname;
    struct addrinfo *ai_next;
};

/* Flags for getaddrinfo */
#define AI_PASSIVE     0x0001
#define AI_CANONNAME   0x0002
#define AI_NUMERICHOST 0x0004

/* Error codes */
#define EAI_AGAIN      1
#define EAI_BADFLAGS   2
#define EAI_FAIL       3
#define EAI_FAMILY     4
#define EAI_MEMORY     5
#define EAI_NONAME     6
#define EAI_SERVICE    8
#define EAI_SOCKTYPE   10
#define EAI_SYSTEM     11

struct hostent *gethostbyname(const char *name);
int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res);
void freeaddrinfo(struct addrinfo *res);
const char *gai_strerror(int errcode);

#endif /* _NETDB_H */
