#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define _SYS_SOCKET_H // Bypass system definitions
#include <sys/types.h>
#include <net/socket.h>

int main() {
    printf("Basic compilation test for lwIP socket layer.\n");
    return 0;
}
