#include <hal.h>
#include <errno.h>
/* removed net/socket.h */
#ifndef O_RDWR
#define O_RDWR 2
#endif
#define MAX_FD 256

#include <string.h>
#include <task.h>
#include <vmm.h>
#include <mm.h>
#include <fs/vfs.h>
#include <lwip/sockets.h>

static ssize_t lwip_socket_read_fs(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    (void)offset;
    if ((node->flags & 0x7) != FS_SOCKET) return 0;
    int res = lwip_recv((int)node->inode, buffer, size, 0);
    return (res < 0) ? 0 : (ssize_t)res;
}

static uint32_t lwip_socket_write_fs(fs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    (void)offset;
    if ((node->flags & 0x7) != FS_SOCKET) return 0;
    int res = lwip_send((int)node->inode, buffer, size, 0);
    return (res < 0) ? 0 : (uint32_t)res;
}

static void lwip_socket_close_fs(fs_node_t* node) {
    if ((node->flags & 0x7) == FS_SOCKET) {
        lwip_close((int)node->inode);
    }
}

int sys_lwip_socket(int domain, int type, int protocol) {
    int sock_id = lwip_socket(domain, type, protocol);
    if (sock_id < 0) return -ENOMEM;

    task_t* current = task_get_current();
    int fd = -1;
    for (int i = 3; i < MAX_FD; i++) {
        if (current->fd_table[i].node == NULL) {
            fd = i;
            break;
        }
    }
    if (fd == -1) {
        lwip_close(sock_id);
        return -EMFILE;
    }

    fs_node_t* node = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!node) {
        lwip_close(sock_id);
        return -ENOMEM;
    }
    memset(node, 0, sizeof(fs_node_t));
    strlcpy(node->name, "socket", 32);
    node->flags = FS_SOCKET;
    node->inode = (uint32_t)sock_id;
    node->read = lwip_socket_read_fs;
    node->write = lwip_socket_write_fs;
    node->close = lwip_socket_close_fs;
    node->ref_count = 1;

    current->fd_table[fd].node = node;
    current->fd_table[fd].offset = 0;
    current->fd_table[fd].flags = O_RDWR;

    return fd;
}

static inline int get_lwip_sock(int fd) {
    task_t* current = task_get_current();
    if (fd < 0 || fd >= MAX_FD) return -1;
    if (!current->fd_table[fd].node) return -1;
    fs_node_t* node = current->fd_table[fd].node;
    if ((node->flags & 0x7) != FS_SOCKET) return -1;
    return (int)node->inode;
}

int sys_lwip_bind(int fd, const void *name, socklen_t namelen) {
    int sock = get_lwip_sock(fd);
    if (sock < 0) return -EBADF;
    return lwip_bind(sock, name, namelen);
}

int sys_lwip_listen(int fd, int backlog) {
    int sock = get_lwip_sock(fd);
    if (sock < 0) return -EBADF;
    return lwip_listen(sock, backlog);
}

int sys_lwip_accept(int fd, void *addr, socklen_t *addrlen) {
    int sock = get_lwip_sock(fd);
    if (sock < 0) return -EBADF;

    int new_sock = lwip_accept(sock, (struct sockaddr*)addr, (socklen_t*)addrlen);
    if (new_sock < 0) return new_sock;

    task_t* current = task_get_current();
    int new_fd = -1;
    for (int i = 3; i < MAX_FD; i++) {
        if (current->fd_table[i].node == NULL) {
            new_fd = i;
            break;
        }
    }
    if (new_fd == -1) {
        lwip_close(new_sock);
        return -EMFILE;
    }

    fs_node_t* node = (fs_node_t*)kmalloc(sizeof(fs_node_t));
    if (!node) {
        lwip_close(new_sock);
        return -ENOMEM;
    }
    memset(node, 0, sizeof(fs_node_t));
    strlcpy(node->name, "socket", 32);
    node->flags = FS_SOCKET;
    node->inode = (uint32_t)new_sock;
    node->read = lwip_socket_read_fs;
    node->write = lwip_socket_write_fs;
    node->close = lwip_socket_close_fs;
    node->ref_count = 1;

    current->fd_table[new_fd].node = node;
    current->fd_table[new_fd].offset = 0;
    current->fd_table[new_fd].flags = O_RDWR;

    return new_fd;
}

int sys_lwip_connect(int fd, const void *name, socklen_t namelen) {
    int sock = get_lwip_sock(fd);
    if (sock < 0) return -EBADF;
    return lwip_connect(sock, name, namelen);
}

ssize_t sys_lwip_sendto(int fd, const void *data, size_t size, int flags, const void *to, socklen_t tolen) {
    int sock = get_lwip_sock(fd);
    if (sock < 0) return -EBADF;
    return lwip_sendto(sock, data, size, flags, to, tolen);
}

ssize_t sys_lwip_recvfrom(int fd, void *mem, size_t len, int flags, void *from, socklen_t *fromlen) {
    int sock = get_lwip_sock(fd);
    if (sock < 0) return -EBADF;
    return lwip_recvfrom(sock, mem, len, flags, (struct sockaddr*)from, (socklen_t*)fromlen);
}

int sys_lwip_poll(struct pollfd *fds, uint32_t nfds, int timeout) {
    if (nfds > MAX_FD) return -EINVAL; /* Prevent kernel memory exhaustion */

    struct pollfd *mapped_fds = kmalloc(nfds * sizeof(struct pollfd));
    if (!mapped_fds) return -ENOMEM;

    for (uint32_t i = 0; i < nfds; i++) {
        mapped_fds[i].fd = get_lwip_sock(fds[i].fd);
        mapped_fds[i].events = fds[i].events;
        mapped_fds[i].revents = 0;
    }

    int res = lwip_poll(mapped_fds, nfds, timeout);

    for (uint32_t i = 0; i < nfds; i++) {
        fds[i].revents = mapped_fds[i].revents;
    }

    kfree(mapped_fds);
    return res;
}

int sys_lwip_select(int maxfdp1, fd_set *readset, fd_set *writeset, fd_set *exceptset, struct timeval *timeout) {
    if (maxfdp1 < 0 || maxfdp1 > MAX_FD) return -EINVAL;
    /* Map VFS FDs to lwIP FDs */
    fd_set mapped_read, mapped_write, mapped_except;
    FD_ZERO(&mapped_read); FD_ZERO(&mapped_write); FD_ZERO(&mapped_except);

    int lwip_maxfd = 0;

    for (int i = 0; i < maxfdp1; i++) {
        if (readset && FD_ISSET(i, readset)) {
            int sock = get_lwip_sock(i);
            if (sock >= 0) { FD_SET(sock, &mapped_read); if (sock > lwip_maxfd) lwip_maxfd = sock; }
        }
        if (writeset && FD_ISSET(i, writeset)) {
            int sock = get_lwip_sock(i);
            if (sock >= 0) { FD_SET(sock, &mapped_write); if (sock > lwip_maxfd) lwip_maxfd = sock; }
        }
        if (exceptset && FD_ISSET(i, exceptset)) {
            int sock = get_lwip_sock(i);
            if (sock >= 0) { FD_SET(sock, &mapped_except); if (sock > lwip_maxfd) lwip_maxfd = sock; }
        }
    }

    int res = lwip_select(lwip_maxfd + 1,
        readset ? &mapped_read : NULL,
        writeset ? &mapped_write : NULL,
        exceptset ? &mapped_except : NULL,
        (void*)timeout);

    /* Map back to VFS FDs */
    if (res > 0) {
        fd_set out_read, out_write, out_except;
        FD_ZERO(&out_read); FD_ZERO(&out_write); FD_ZERO(&out_except);

        for (int i = 0; i < maxfdp1; i++) {
            int sock = get_lwip_sock(i);
            if (sock >= 0) {
                if (readset && FD_ISSET(sock, &mapped_read)) FD_SET(i, &out_read);
                if (writeset && FD_ISSET(sock, &mapped_write)) FD_SET(i, &out_write);
                if (exceptset && FD_ISSET(sock, &mapped_except)) FD_SET(i, &out_except);
            }
        }
        if (readset) *readset = out_read;
        if (writeset) *writeset = out_write;
        if (exceptset) *exceptset = out_except;
    }

    return res;
}

int sys_lwip_setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen) {
    int sock = get_lwip_sock(fd);
    if (sock < 0) return -EBADF;
    return lwip_setsockopt(sock, level, optname, optval, optlen);
}

int sys_lwip_getsockopt(int fd, int level, int optname, void *optval, socklen_t *optlen) {
    int sock = get_lwip_sock(fd);
    if (sock < 0) return -EBADF;
    return lwip_getsockopt(sock, level, optname, optval, (socklen_t*)optlen);
}

int sys_lwip_getpeername(int fd, void *name, socklen_t *namelen) {
    int sock = get_lwip_sock(fd);
    if (sock < 0) return -EBADF;
    return lwip_getpeername(sock, (struct sockaddr*)name, (socklen_t*)namelen);
}

int sys_lwip_getsockname(int fd, void *name, socklen_t *namelen) {
    int sock = get_lwip_sock(fd);
    if (sock < 0) return -EBADF;
    return lwip_getsockname(sock, (struct sockaddr*)name, (socklen_t*)namelen);
}

int sys_lwip_shutdown(int fd, int how) {
    int sock = get_lwip_sock(fd);
    if (sock < 0) return -EBADF;
    return lwip_shutdown(sock, how);
}

ssize_t sys_lwip_sendmsg(int fd, const void *msg, int flags) {
    int sock = get_lwip_sock(fd);
    if (sock < 0) return -EBADF;
    return lwip_sendmsg(sock, msg, flags);
}

ssize_t sys_lwip_recvmsg(int fd, void *msg, int flags) {
    int sock = get_lwip_sock(fd);
    if (sock < 0) return -EBADF;
    return lwip_recvmsg(sock, msg, flags);
}

ssize_t sys_lwip_send(int fd, const void *buf, size_t len, int flags) {
    return sys_lwip_sendto(fd, buf, len, flags, NULL, 0);
}

ssize_t sys_lwip_recv(int fd, void *buf, size_t len, int flags) {
    return sys_lwip_recvfrom(fd, buf, len, flags, NULL, NULL);
}
int sys_lwip_close(int fd) {
    task_t* current = task_get_current();
    if (fd < 0 || fd >= MAX_FD) return -EBADF;
    if (!current->fd_table[fd].node) return -EBADF;
    fs_node_t* node = current->fd_table[fd].node;
    current->fd_table[fd].node = NULL;
    if (__atomic_sub_fetch(&node->ref_count, 1, __ATOMIC_SEQ_CST) == 0) {
        if (node->close) node->close(node);
        kfree(node);
    }
    return 0;
}
