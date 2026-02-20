#include <netinet/in.h>
#include <stdint.h>

/* Byte Order Functions */
uint16_t htons(uint16_t v) {
    return (v << 8) | (v >> 8);
}

uint16_t ntohs(uint16_t v) {
    return (v << 8) | (v >> 8);
}

uint32_t htonl(uint32_t v) {
    return ((v & 0xFF) << 24) | ((v & 0xFF00) << 8) | ((v & 0xFF0000) >> 8) | ((v >> 24) & 0xFF);
}

uint32_t ntohl(uint32_t v) {
    return htonl(v);
}
