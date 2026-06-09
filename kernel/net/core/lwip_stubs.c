#include <types.h>
#include <fs/vfs.h>
#include <task.h>
#include <mm.h>
#include <string.h>
#include <net/socket.h>
#include <hal.h>

#define MAX_FD 256
#ifndef O_RDWR
#define O_RDWR 2
#endif

static ssize_t native_socket_read_fs(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    (void)offset;
    if ((node->flags & 0x7) != FS_SOCKET) return 0;
    int res = net_recv((int)node->inode, buffer, size, 0);
    return (res < 0) ? 0 : (ssize_t)res;
}

static uint32_t native_socket_write_fs(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    (void)offset;
    if ((node->flags & 0x7) != FS_SOCKET) return 0;
    int res = net_send((int)node->inode, buffer, size, 0);
    return (res < 0) ? 0 : (uint32_t)res;
}

static void native_socket_close_fs(fs_node_t* node) {
    if ((node->flags & 0x7) == FS_SOCKET) {
        net_close((int)node->inode);
    }
}

int sys_lwip_socket(int domain, int type, int protocol) {
    int sock_id = net_socket(domain, type, protocol);
    if (sock_id < 0) {
        hal_console_write("[DEBUG] net_socket failed\n");
        return -12;
    }

    task_t* current = task_get_current();
    int fd = -1;
    for (int i = 3; i < MAX_FD; i++) {
        if (current->fd_table[i].node == NULL) {
            fd = i;
            break;
        }
    }
    if (fd == -1) {
        hal_console_write("[DEBUG] fd allocation failed\n");
        net_close(sock_id);
        return -24;
    }

    fs_node_t* node = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!node) {
        net_close(sock_id);
        return -12;
    }
    memset(node, 0, sizeof(fs_node_t));
    strlcpy(node->name, "socket", 32);
    node->flags = FS_SOCKET;
    node->inode = (uint32_t)sock_id;
    node->read = native_socket_read_fs;
    node->write = native_socket_write_fs;
    node->close = native_socket_close_fs;
    node->ref_count = 1;

    current->fd_table[fd].node = node;
    current->fd_table[fd].offset = 0;
    current->fd_table[fd].flags = O_RDWR;

    return fd;
}

static inline int get_native_sock(int fd) {
    task_t* current = task_get_current();
    if (fd < 0 || fd >= MAX_FD) return -1;
    if (!current->fd_table[fd].node) return -1;
    fs_node_t* node = current->fd_table[fd].node;
    if ((node->flags & 0x7) != FS_SOCKET) return -1;
    return (int)node->inode;
}

int sys_lwip_connect(int fd, const struct sockaddr_old *name, uint32_t namelen) {
    int sock = get_native_sock(fd);
    if (sock < 0) return -9;
    return net_connect(sock, (const struct sockaddr_in_old*)name, namelen);
}

ssize_t sys_lwip_sendto(int fd, const void *dataptr, size_t size, int flags, const struct sockaddr_old *to, uint32_t tolen) {
    (void)to; (void)tolen;
    int sock = get_native_sock(fd);
    if (sock < 0) return -9;
    return net_send(sock, dataptr, size, flags);
}

ssize_t sys_lwip_recvfrom(int fd, void *mem, size_t len, int flags, struct sockaddr_old *from, uint32_t *fromlen) {
    (void)from; (void)fromlen;
    int sock = get_native_sock(fd);
    if (sock < 0) return -9;
    return net_recv(sock, mem, len, flags);
}

int sys_lwip_close(int fd) {
    task_t* current = task_get_current();
    if (fd < 0 || fd >= MAX_FD) return -9;
    if (!current->fd_table[fd].node) return -9;
    fs_node_t* node = current->fd_table[fd].node;
    current->fd_table[fd].node = NULL;
    if (__atomic_sub_fetch(&node->ref_count, 1, __ATOMIC_SEQ_CST) == 0) {
        if (node->close) node->close(node);
        kfree(node);
    }
    return 0;
}

void net_dhcp_renew(void) {}
int sys_lwip_getsockopt(int s, int level, int optname, void *optval, uint32_t *optlen) { (void)s; (void)level; (void)optname; (void)optval; (void)optlen; return -1; }
int sys_lwip_setsockopt(int s, int level, int optname, const void *optval, uint32_t optlen) { (void)s; (void)level; (void)optname; (void)optval; (void)optlen; return -1; }
int sys_lwip_getpeername(int s, struct sockaddr_old *name, uint32_t *namelen) { (void)s; (void)name; (void)namelen; return -1; }
int sys_lwip_getsockname(int s, struct sockaddr_old *name, uint32_t *namelen) { (void)s; (void)name; (void)namelen; return -1; }
int sys_lwip_listen(int s, int backlog) { (void)s; (void)backlog; return -1; }
int sys_lwip_bind(int s, const struct sockaddr_old *name, uint32_t namelen) { (void)s; (void)name; (void)namelen; return -1; }
int sys_lwip_accept(int s, struct sockaddr_old *addr, uint32_t *addrlen) { (void)s; (void)addr; (void)addrlen; return -1; }
int net_gethostbyname(const char *name, uint32_t *out_ip) { (void)name; (void)out_ip; return -1; }
