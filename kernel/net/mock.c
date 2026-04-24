
#include <types.h>

int net_sem = 0;
void* socket_table = 0;


int net_socket(int domain, int type, int protocol) { (void)domain; (void)type; (void)protocol; return -1; }
int net_connect(int sock_id, uint32_t ip, uint16_t port) { (void)sock_id; (void)ip; (void)port; return -1; }
int net_send(int sock_id, const void *data, size_t size) { (void)sock_id; (void)data; (void)size; return -1; }
int net_recv(int sock_id, void *data, size_t size) { (void)sock_id; (void)data; (void)size; return -1; }
int net_close(int sock_id) { (void)sock_id; return -1; }
