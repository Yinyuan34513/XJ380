#define ALL_IMPLEMENTATION

#include <dlinker.h>
#include <cpu/lock.h>
#include <errno.h>
#include <fs/vfs/vfs.h>
#include <krlibc.h>
#include <net/socket.h>
#include <syscall/syscall.h>
#include <task/poll.h>
#include <task/pcb.h>

typedef struct socketfs_handle {
    socket_provider_t provider;
    void             *socket;
} socketfs_handle_t;

#define NETLINK_SOCKET_QUEUE_SIZE 1024
#define NETLINK_UEVENT_HISTORY_SIZE 1024
#define NETLINK_UEVENT_GROUP 1U

typedef struct netlink_socket {
    int      protocol;
    int      type;
    uint32_t pid;
    uint32_t groups;
    uint64_t flags;
    bool     passcred;
    int      recv_buffer_size;
    int      send_buffer_size;
    spin_t   lock;
    size_t   head;
    size_t   tail;
    size_t   count;
    char    *messages[NETLINK_SOCKET_QUEUE_SIZE];
    size_t   lengths[NETLINK_SOCKET_QUEUE_SIZE];
} netlink_socket_t;

typedef struct netlink_history_entry {
    char  *message;
    size_t length;
    bool   valid;
} netlink_history_entry_t;

static int        socketfs_id      = 0;
static int        socketfs_next_id = 0;
static vfs_node_t socketfs_root    = NULL;

static spin_t           g_socket_provider_lock  = SPIN_INIT;
static socket_provider_t g_socket_provider_data = {};
static bool              g_socket_provider_ready = false;
static spin_t            g_netlink_lock = SPIN_INIT;
static netlink_socket_t *g_netlink_sockets[64];
static uint32_t          g_netlink_next_pid = 1;
static spin_t            g_netlink_history_lock = SPIN_INIT;
static netlink_history_entry_t g_netlink_uevent_history[NETLINK_UEVENT_HISTORY_SIZE];
static size_t                  g_netlink_uevent_history_next = 0;

static inline bool socketfs_provider_valid(const socket_provider_t *provider)
{
    return provider != NULL && provider->create != NULL && provider->close != NULL &&
           provider->read != NULL && provider->write != NULL && provider->bind != NULL &&
           provider->recvfrom != NULL && provider->sendto != NULL &&
           provider->getsockname != NULL && provider->getpeername != NULL &&
           provider->getsockopt != NULL && provider->setsockopt != NULL &&
           provider->connect != NULL && provider->listen != NULL && provider->accept != NULL &&
           provider->shutdown != NULL && provider->poll != NULL && provider->set_flags != NULL &&
           provider->resolve_ipv4 != NULL && provider->resolve_ipv6 != NULL &&
           provider->set_dns_server != NULL &&
           provider->get_dns_server != NULL && provider->set_ipv4_config != NULL &&
           provider->get_netinfo != NULL;
}

static int socketfs_snapshot_provider(socket_provider_t *out_provider)
{
    if (out_provider == NULL) {
        return -EINVAL;
    }

    spin_lock(&g_socket_provider_lock);
    if (!g_socket_provider_ready || !socketfs_provider_valid(&g_socket_provider_data)) {
        memset(out_provider, 0, sizeof(*out_provider));
        spin_unlock(&g_socket_provider_lock);
        return -ENOSYS;
    }
    *out_provider = g_socket_provider_data;
    spin_unlock(&g_socket_provider_lock);
    return 0;
}

static inline socketfs_handle_t *socketfs_get_handle(vfs_node_t node)
{
    if (node == NULL || !(node->type & file_socket) || node->handle == NULL) {
        return NULL;
    }
    return (socketfs_handle_t *)node->handle;
}

static inline fd_file_handle *socketfs_get_fd_handle(int fd)
{
    tcb_t current = get_current_task();
    if (current == NULL || current->parent_group == NULL || current->parent_group->file_open == NULL) {
        return NULL;
    }
    if (fd < 0) {
        return NULL;
    }
    return (fd_file_handle *)queue_get(current->parent_group->file_open, fd);
}

static inline bool socketfs_is_socket_fd(int fd, fd_file_handle **out_handle)
{
    fd_file_handle *handle = socketfs_get_fd_handle(fd);
    if (out_handle != NULL) {
        *out_handle = handle;
    }
    return handle != NULL && handle->node != NULL && (handle->node->type & file_socket);
}

static inline void socketfs_sync_flags(socketfs_handle_t *handle, uint64_t flags)
{
    if (handle != NULL && handle->provider.set_flags != NULL) {
        handle->provider.set_flags(handle->socket, flags);
    }
}

static inline int socketfs_errno(ssize_t ret)
{
    return ret >= 0 ? 0 : (int)ret;
}

static errno_t socketfs_mount(const char *handle, vfs_node_t node)
{
    if ((uint64_t)handle != NETFS_REGISTER_ID) {
        return VFS_STATUS_FAILED;
    }
    node->fsid   = socketfs_id;
    node->type   = file_dir;
    node->handle = NULL;
    socketfs_root = node;
    return VFS_STATUS_SUCCESS;
}

static void socketfs_open(void *parent, const char *name, vfs_node_t node)
{
    (void)parent;
    (void)name;
    if (node == socketfs_root) {
        node->type = file_dir;
        return;
    }
    node->type = file_socket;
    node->size = (uint64_t)-1;
}

static errno_t socketfs_stat(void *file, vfs_node_t node)
{
    (void)file;
    node->type = file_socket;
    node->size = (uint64_t)-1;
    node->fsid = socketfs_id;
    return EOK;
}

static size_t socketfs_read_cb(void *file, void *addr, size_t offset, size_t size)
{
    (void)offset;
    socketfs_handle_t *handle = (socketfs_handle_t *)file;
    if (handle == NULL || handle->provider.read == NULL) {
        return VFS_STATUS_FAILED;
    }
    ssize_t ret = handle->provider.read(handle->socket, addr, size, 0);
    return ret < 0 ? VFS_STATUS_FAILED : (size_t)ret;
}

static size_t socketfs_write_cb(void *file, const void *addr, size_t offset, size_t size)
{
    (void)offset;
    socketfs_handle_t *handle = (socketfs_handle_t *)file;
    if (handle == NULL || handle->provider.write == NULL) {
        return VFS_STATUS_FAILED;
    }
    ssize_t ret = handle->provider.write(handle->socket, addr, size, 0);
    return ret < 0 ? VFS_STATUS_FAILED : (size_t)ret;
}

static errno_t socketfs_ioctl(void *file, size_t req, void *arg)
{
    (void)file;
    (void)req;
    (void)arg;
    return -ENOTTY;
}

static errno_t socketfs_poll_cb(void *file, size_t events)
{
    socketfs_handle_t *handle = (socketfs_handle_t *)file;
    if (handle == NULL || handle->provider.poll == NULL) {
        return -EINVAL;
    }
    return handle->provider.poll(handle->socket, events);
}

static errno_t socketfs_del(void *parent, vfs_node_t node)
{
    (void)parent;
    socketfs_handle_t *handle = socketfs_get_handle(node);
    if (handle != NULL) {
        if (socketfs_provider_valid(&handle->provider)) {
            handle->provider.close(handle->socket);
        }
        free(handle);
        node->handle = NULL;
    }
    return EOK;
}

static int socketfs_dummy()
{
    return -ENOSYS;
}

static struct vfs_callback socketfs_callbacks = {
    .mount    = socketfs_mount,
    .unmount  = (vfs_unmount_t)empty,
    .open     = socketfs_open,
    .close    = (vfs_close_t)empty,
    .read     = socketfs_read_cb,
    .write    = socketfs_write_cb,
    .readlink = (vfs_readlink_t)socketfs_dummy,
    .mkdir    = (vfs_mk_t)empty,
    .mkfile   = (vfs_mk_t)empty,
    .link     = (vfs_mk_t)socketfs_dummy,
    .symlink  = (vfs_mk_t)socketfs_dummy,
    .stat     = socketfs_stat,
    .ioctl    = socketfs_ioctl,
    .dup      = (vfs_dup_t)empty,
    .poll     = socketfs_poll_cb,
    .map      = (vfs_mapfile_t)empty,
    .resize   = (vfs_resize_t)socketfs_dummy,
    .del      = socketfs_del,
    .rename   = (vfs_rename_t)empty,
};

int socketfs_alloc_fd_with_provider(void *impl, uint64_t flags, const socket_provider_t *provider)
{
    if (socketfs_root == NULL || impl == NULL || provider == NULL) {
        return -ENOSYS;
    }

    char name[32];
    snprintf(name, sizeof(name), "sock%d", socketfs_next_id++);

    vfs_node_t node = vfs_node_alloc(socketfs_root, name);
    if (node == NULL) {
        return -ENOMEM;
    }

    socketfs_handle_t *socket_handle = (socketfs_handle_t *)calloc(1, sizeof(socketfs_handle_t));
    if (socket_handle == NULL) {
        socketfs_root->child = list_delete(socketfs_root->child, node);
        free(node->name);
        free(node);
        return -ENOMEM;
    }

    socket_handle->provider = *provider;
    socket_handle->socket   = impl;

    node->type   = file_socket | file_delete;
    node->fsid   = socketfs_id;
    node->handle = socket_handle;
    node->size   = (uint64_t)-1;
    node->flags  = flags;
    node->mode   = 0777;

    fd_file_handle *fd_handle = (fd_file_handle *)calloc(1, sizeof(fd_file_handle));
    if (fd_handle == NULL) {
        socketfs_del(NULL, node);
        socketfs_root->child = list_delete(socketfs_root->child, node);
        free(node->name);
        free(node);
        return -ENOMEM;
    }

    fd_handle->node   = node;
    fd_handle->offset = 0;
    fd_handle->flags  = flags;
    fd_handle->fd     = queue_enqueue_lowest(get_current_task()->parent_group->file_open, fd_handle);
    if ((int)fd_handle->fd < 0) {
        free(fd_handle);
        socketfs_del(NULL, node);
        socketfs_root->child = list_delete(socketfs_root->child, node);
        free(node->name);
        free(node);
        return -EMFILE;
    }

    return (int)fd_handle->fd;
}

int socketfs_get_socket_impl(int fd, void **out_impl, socket_provider_t *out_provider)
{
    fd_file_handle *fd_handle = NULL;
    if (!socketfs_is_socket_fd(fd, &fd_handle)) {
        return -EBADF;
    }

    socketfs_handle_t *handle = socketfs_get_handle(fd_handle->node);
    if (handle == NULL) {
        return -EBADF;
    }

    if (out_impl != NULL) {
        *out_impl = handle->socket;
    }
    if (out_provider != NULL) {
        *out_provider = handle->provider;
    }
    return 0;
}

static int socketfs_alloc_fd(void *impl, uint64_t flags)
{
    socket_provider_t provider = {};
    int ret = socketfs_snapshot_provider(&provider);
    if (ret < 0) return ret;
    return socketfs_alloc_fd_with_provider(impl, flags, &provider);
}

static bool netlink_groups_match(uint32_t subscribed_groups, uint32_t target_groups)
{
    return target_groups != 0 && (subscribed_groups & target_groups) != 0;
}

static bool netlink_queue_message_locked(netlink_socket_t *sock, const char *buf, size_t len)
{
    if (sock == NULL || buf == NULL || len == 0) return false;

    if (sock->count == sizeof(sock->messages) / sizeof(sock->messages[0]))
    {
        free(sock->messages[sock->head]);
        sock->messages[sock->head] = NULL;
        sock->lengths[sock->head] = 0;
        sock->head = (sock->head + 1) % (sizeof(sock->messages) / sizeof(sock->messages[0]));
        sock->count--;
    }

    char *copy = (char *)malloc(len + 1);
    if (copy == NULL) return false;

    memcpy(copy, buf, len);
    copy[len] = '\0';
    sock->messages[sock->tail] = copy;
    sock->lengths[sock->tail] = len;
    sock->tail = (sock->tail + 1) % (sizeof(sock->messages) / sizeof(sock->messages[0]));
    sock->count++;
    return true;
}

static void netlink_uevent_history_add(const char *buf, size_t len)
{
    if (buf == NULL || len == 0) return;

    char *copy = (char *)malloc(len + 1);
    if (copy == NULL) return;

    memcpy(copy, buf, len);
    copy[len] = '\0';

    spin_lock(&g_netlink_history_lock);
    netlink_history_entry_t *entry = &g_netlink_uevent_history[g_netlink_uevent_history_next];
    free(entry->message);
    entry->message = copy;
    entry->length = len;
    entry->valid = true;
    g_netlink_uevent_history_next = (g_netlink_uevent_history_next + 1) % NETLINK_UEVENT_HISTORY_SIZE;
    spin_unlock(&g_netlink_history_lock);
}

static size_t netlink_deliver_uevent_history(netlink_socket_t *sock)
{
    if (sock == NULL || sock->protocol != NETLINK_KOBJECT_UEVENT) return 0;

    spin_lock(&sock->lock);
    uint32_t groups = sock->groups;
    spin_unlock(&sock->lock);
    if (!netlink_groups_match(groups, NETLINK_UEVENT_GROUP)) return 0;

    size_t delivered = 0;
    spin_lock(&g_netlink_history_lock);
    size_t start = g_netlink_uevent_history_next;
    for (size_t i = 0; i < NETLINK_UEVENT_HISTORY_SIZE; i++)
    {
        netlink_history_entry_t *entry = &g_netlink_uevent_history[(start + i) % NETLINK_UEVENT_HISTORY_SIZE];
        if (!entry->valid || entry->message == NULL || entry->length == 0) continue;

        spin_lock(&sock->lock);
        if (netlink_queue_message_locked(sock, entry->message, entry->length)) delivered++;
        spin_unlock(&sock->lock);
    }
    spin_unlock(&g_netlink_history_lock);

    write_serial_fmt("[udev-trace] netlink:history-deliver sock=%p groups=0x%x delivered=%llu queued=%llu\n",
                     sock,
                     groups,
                     (unsigned long long)delivered,
                     (unsigned long long)sock->count);
    return delivered;
}

static ssize_t netlink_read_impl(netlink_socket_t *sock, void *buffer, size_t size, uint64_t flags)
{
    if (sock == NULL || (buffer == NULL && size > 0)) return -EINVAL;
    while (true)
    {
        spin_lock(&sock->lock);
        if (sock->count != 0) break;
        spin_unlock(&sock->lock);
        if (((flags | sock->flags) & O_NONBLOCK) || (flags & MSG_DONTWAIT)) return -EAGAIN;
        scheduler_sleep_ns(1000000ULL);
    }

    char *msg = sock->messages[sock->head];
    size_t len = sock->lengths[sock->head];
    size_t copy = MIN(size, len);
    if (copy > 0) memcpy(buffer, msg, copy);
    free(msg);
    sock->messages[sock->head] = NULL;
    sock->lengths[sock->head] = 0;
    sock->head = (sock->head + 1) % (sizeof(sock->messages) / sizeof(sock->messages[0]));
    sock->count--;
    spin_unlock(&sock->lock);
    return (ssize_t)copy;
}

static int netlink_close_impl(void *socket)
{
    netlink_socket_t *sock = (netlink_socket_t *)socket;
    if (sock == NULL) return 0;

    spin_lock(&g_netlink_lock);
    for (size_t i = 0; i < sizeof(g_netlink_sockets) / sizeof(g_netlink_sockets[0]); i++)
    {
        if (g_netlink_sockets[i] == sock) g_netlink_sockets[i] = NULL;
    }
    spin_unlock(&g_netlink_lock);

    spin_lock(&sock->lock);
    for (size_t i = 0; i < sizeof(sock->messages) / sizeof(sock->messages[0]); i++)
        free(sock->messages[i]);
    spin_unlock(&sock->lock);
    free(sock);
    return 0;
}

static int netlink_create_impl(int domain, int type, int protocol, void **out_socket)
{
    if (domain != AF_NETLINK || out_socket == NULL) return -EAFNOSUPPORT;
    int sock_type = type & SOCK_TYPE_MASK;
    if (sock_type != SOCK_RAW && sock_type != SOCK_DGRAM) return -EPROTONOSUPPORT;
    if (protocol != NETLINK_KOBJECT_UEVENT && protocol != NETLINK_ROUTE && protocol != NETLINK_GENERIC)
        return -EPROTONOSUPPORT;
    netlink_socket_t *sock = (netlink_socket_t *)calloc(1, sizeof(netlink_socket_t));
    if (sock == NULL) return -ENOMEM;
    sock->protocol = protocol;
    sock->type = sock_type;
    sock->pid = __atomic_fetch_add(&g_netlink_next_pid, 1, __ATOMIC_SEQ_CST);
    sock->recv_buffer_size = 65536;
    sock->send_buffer_size = 65536;
    sock->lock = SPIN_INIT;
    spin_lock(&g_netlink_lock);
    for (size_t i = 0; i < sizeof(g_netlink_sockets) / sizeof(g_netlink_sockets[0]); i++)
    {
        if (g_netlink_sockets[i] == NULL)
        {
            g_netlink_sockets[i] = sock;
            break;
        }
    }
    spin_unlock(&g_netlink_lock);
    *out_socket = sock;
    write_serial_fmt("[udev-trace] netlink:create type=%d protocol=%d pid=%u sock=%p\n",
                     sock_type,
                     protocol,
                     sock->pid,
                     sock);
    return 0;
}

static ssize_t netlink_read_provider(void *socket, void *buffer, size_t size, uint64_t flags)
{
    return netlink_read_impl((netlink_socket_t *)socket, buffer, size, flags);
}

static ssize_t netlink_write_provider(void *socket, const void *buffer, size_t size, uint64_t flags)
{
    (void)socket;
    (void)buffer;
    (void)flags;
    return (ssize_t)size;
}

static ssize_t netlink_recvfrom_provider(void *socket, void *buffer, size_t size, uint64_t flags,
                                         struct sockaddr *addr, socklen_t *addrlen)
{
    ssize_t ret = netlink_read_impl((netlink_socket_t *)socket, buffer, size, flags);
    netlink_socket_t *sock = (netlink_socket_t *)socket;
    write_serial_fmt("[udev-trace] netlink:recvfrom sock=%p protocol=%d flags=0x%llx req=%llu ret=%lld remaining=%llu\n",
                     sock,
                     sock != NULL ? sock->protocol : -1,
                     (unsigned long long)flags,
                     (unsigned long long)size,
                     (long long)ret,
                     sock != NULL ? (unsigned long long)sock->count : 0);
    if (ret >= 0 && addr != NULL && addrlen != NULL && *addrlen >= sizeof(struct sockaddr_nl))
    {
        struct sockaddr_nl nl = {};
        nl.nl_family = AF_NETLINK;
        nl.nl_pid = 0;
        nl.nl_groups = 1;
        memcpy(addr, &nl, sizeof(nl));
        *addrlen = sizeof(nl);
    }
    return ret;
}

static ssize_t netlink_sendto_provider(void *socket, const void *buffer, size_t size, uint64_t flags,
                                       const struct sockaddr *addr, socklen_t addrlen)
{
    (void)buffer;
    (void)addr;
    (void)addrlen;
    netlink_socket_t *sock = (netlink_socket_t *)socket;
    write_serial_fmt("[udev-trace] netlink:sendto sock=%p protocol=%d flags=0x%llx size=%llu\n",
                     sock,
                     sock != NULL ? sock->protocol : -1,
                     (unsigned long long)flags,
                     (unsigned long long)size);
    return (ssize_t)size;
}

static int netlink_bind_provider(void *socket, const struct sockaddr *addr, socklen_t addrlen)
{
    netlink_socket_t *sock = (netlink_socket_t *)socket;
    if (sock == NULL || addr == NULL || addrlen < sizeof(struct sockaddr_nl)) return -EINVAL;
    const struct sockaddr_nl *nl = (const struct sockaddr_nl *)addr;
    if (nl->nl_family != AF_NETLINK) return -EAFNOSUPPORT;
    sock->groups = nl->nl_groups;
    if (nl->nl_pid != 0) sock->pid = nl->nl_pid;
    write_serial_fmt("[udev-trace] netlink:bind sock=%p protocol=%d pid=%u groups=0x%x\n",
                     sock,
                     sock->protocol,
                     sock->pid,
                     sock->groups);

    spin_lock(&g_netlink_lock);
    bool present = false;
    for (size_t i = 0; i < sizeof(g_netlink_sockets) / sizeof(g_netlink_sockets[0]); i++)
        if (g_netlink_sockets[i] == sock) present = true;
    if (!present)
    {
        for (size_t i = 0; i < sizeof(g_netlink_sockets) / sizeof(g_netlink_sockets[0]); i++)
        {
            if (g_netlink_sockets[i] == NULL)
            {
                g_netlink_sockets[i] = sock;
                break;
            }
        }
    }
    spin_unlock(&g_netlink_lock);
    netlink_deliver_uevent_history(sock);
    return 0;
}

static int netlink_getsockname_provider(void *socket, struct sockaddr *addr, socklen_t *addrlen)
{
    netlink_socket_t *sock = (netlink_socket_t *)socket;
    if (sock == NULL || addrlen == NULL) return -EINVAL;
    if (addr != NULL && *addrlen >= sizeof(struct sockaddr_nl))
    {
        struct sockaddr_nl nl = {};
        nl.nl_family = AF_NETLINK;
        nl.nl_pid = sock->pid;
        nl.nl_groups = sock->groups;
        memcpy(addr, &nl, sizeof(nl));
    }
    *addrlen = sizeof(struct sockaddr_nl);
    return 0;
}

static int netlink_getsockopt_provider(void *socket, int level, int optname, void *optval, socklen_t *optlen)
{
    netlink_socket_t *sock = (netlink_socket_t *)socket;
    if (sock == NULL) return -EINVAL;
    if (optval == NULL || optlen == NULL || *optlen < sizeof(int)) return -EINVAL;
    int value = 0;
    if (level == SOL_SOCKET)
    {
        switch (optname)
        {
        case SO_TYPE:
            value = sock->type;
            break;
        case SO_PROTOCOL:
            value = sock->protocol;
            break;
        case SO_DOMAIN:
            value = AF_NETLINK;
            break;
        case SO_PASSCRED:
            value = sock->passcred ? 1 : 0;
            break;
        case SO_RCVBUF:
        case SO_RCVBUFFORCE:
            value = sock->recv_buffer_size;
            break;
        case SO_SNDBUF:
        case SO_SNDBUFFORCE:
            value = sock->send_buffer_size;
            break;
        case SO_ERROR:
        case SO_REUSEADDR:
            value = 0;
            break;
        default:
            return -ENOPROTOOPT;
        }
    }
    else if (level != SOL_NETLINK)
    {
        return -ENOPROTOOPT;
    }
    memcpy(optval, &value, sizeof(value));
    *optlen = sizeof(value);
    return 0;
}

static int netlink_setsockopt_provider(void *socket, int level, int optname, const void *optval, socklen_t optlen)
{
    netlink_socket_t *sock = (netlink_socket_t *)socket;
    if (sock == NULL) return -EINVAL;
    if (level == SOL_NETLINK)
    {
        if (optname != NETLINK_ADD_MEMBERSHIP && optname != NETLINK_DROP_MEMBERSHIP) return -ENOPROTOOPT;
        if (optval == NULL || optlen < (socklen_t)sizeof(uint32_t)) return -EINVAL;
        uint32_t group = *(const uint32_t *)optval;
        if (group == 0 || group > 32) return -EINVAL;
        uint32_t bit = 1U << (group - 1);
        bool deliver_history = false;
        spin_lock(&sock->lock);
        if (optname == NETLINK_ADD_MEMBERSHIP)
        {
            uint32_t old_groups = sock->groups;
            sock->groups |= bit;
            deliver_history = sock->protocol == NETLINK_KOBJECT_UEVENT &&
                              netlink_groups_match(bit, NETLINK_UEVENT_GROUP) &&
                              !netlink_groups_match(old_groups, NETLINK_UEVENT_GROUP);
        }
        else
        {
            sock->groups &= ~bit;
        }
        spin_unlock(&sock->lock);
        if (deliver_history) netlink_deliver_uevent_history(sock);
        return 0;
    }

    if (level != SOL_SOCKET) return -ENOPROTOOPT;
    switch (optname)
    {
    case SO_PASSCRED:
        if (optval == NULL || optlen < (socklen_t)sizeof(int)) return -EINVAL;
        spin_lock(&sock->lock);
        sock->passcred = (*(const int *)optval) != 0;
        spin_unlock(&sock->lock);
        return 0;
    case SO_RCVBUF:
    case SO_RCVBUFFORCE:
    case SO_SNDBUF:
    case SO_SNDBUFFORCE: {
        if (optval == NULL || optlen < (socklen_t)sizeof(int)) return -EINVAL;
        int value = *(const int *)optval;
        if (value <= 0) value = 65536;
        spin_lock(&sock->lock);
        if (optname == SO_RCVBUF || optname == SO_RCVBUFFORCE) sock->recv_buffer_size = value;
        else sock->send_buffer_size = value;
        spin_unlock(&sock->lock);
        return 0;
    }
    case SO_ATTACH_FILTER:
    case SO_DETACH_FILTER:
    case SO_REUSEADDR:
        return 0;
    default:
        return -ENOPROTOOPT;
    }
}

static int netlink_poll_provider(void *socket, size_t events)
{
    netlink_socket_t *sock = (netlink_socket_t *)socket;
    if (sock == NULL) return EPOLLERR;
    int revents = 0;
    spin_lock(&sock->lock);
    if ((events & EPOLLIN) && sock->count != 0) revents |= EPOLLIN;
    if (events & EPOLLOUT) revents |= EPOLLOUT;
    spin_unlock(&sock->lock);
    return revents;
}

static int netlink_set_flags_provider(void *socket, uint64_t flags)
{
    netlink_socket_t *sock = (netlink_socket_t *)socket;
    if (sock == NULL) return -EINVAL;
    sock->flags = flags;
    return 0;
}

static int netlink_ok0(void *socket)
{
    (void)socket;
    return 0;
}

static int netlink_connect_provider(void *socket, const struct sockaddr *addr, socklen_t addrlen)
{
    (void)socket;
    (void)addr;
    (void)addrlen;
    return 0;
}

static int netlink_listen_provider(void *socket, int backlog)
{
    (void)socket;
    (void)backlog;
    return -EOPNOTSUPP;
}

static int netlink_accept_provider(void *socket, void **out_socket, struct sockaddr *addr, socklen_t *addrlen)
{
    (void)socket;
    (void)out_socket;
    (void)addr;
    (void)addrlen;
    return -EOPNOTSUPP;
}

static int netlink_shutdown_provider(void *socket, int how)
{
    (void)socket;
    (void)how;
    return 0;
}

static int netlink_resolve_ipv4_provider(const char *hostname, uint32_t *out_addr)
{
    (void)hostname;
    (void)out_addr;
    return -EAFNOSUPPORT;
}

static int netlink_resolve_ipv6_provider(const char *hostname, uint8_t out_addr[16])
{
    (void)hostname;
    (void)out_addr;
    return -EAFNOSUPPORT;
}

static int netlink_set_dns_provider(uint32_t addr)
{
    (void)addr;
    return -EAFNOSUPPORT;
}

static int netlink_get_dns_provider(uint32_t *out_addr)
{
    (void)out_addr;
    return -EAFNOSUPPORT;
}

static int netlink_ipv4_config_provider(const socket_ipv4_config_t *config)
{
    (void)config;
    return -EAFNOSUPPORT;
}

static int netlink_netinfo_provider(socket_netinfo_t *out_info)
{
    (void)out_info;
    return -EAFNOSUPPORT;
}

static socket_provider_t g_netlink_provider = {
    .create = netlink_create_impl,
    .close = netlink_close_impl,
    .read = netlink_read_provider,
    .write = netlink_write_provider,
    .recvfrom = netlink_recvfrom_provider,
    .sendto = netlink_sendto_provider,
    .bind = netlink_bind_provider,
    .connect = netlink_connect_provider,
    .getsockname = netlink_getsockname_provider,
    .getpeername = netlink_getsockname_provider,
    .getsockopt = netlink_getsockopt_provider,
    .setsockopt = netlink_setsockopt_provider,
    .listen = netlink_listen_provider,
    .accept = netlink_accept_provider,
    .shutdown = netlink_shutdown_provider,
    .poll = netlink_poll_provider,
    .set_flags = netlink_set_flags_provider,
    .resolve_ipv4 = netlink_resolve_ipv4_provider,
    .resolve_ipv6 = netlink_resolve_ipv6_provider,
    .set_dns_server = netlink_set_dns_provider,
    .get_dns_server = netlink_get_dns_provider,
    .set_ipv4_config = netlink_ipv4_config_provider,
    .get_netinfo = netlink_netinfo_provider,
};

int netlink_open(int type, int protocol)
{
    void *impl = NULL;
    int ret = netlink_create_impl(AF_NETLINK, type, protocol, &impl);
    if (ret < 0) return ret;
    return socketfs_alloc_fd_with_provider(impl, 0, &g_netlink_provider);
}

void netlink_kernel_uevent_send(const char *buf, int len)
{
    if (buf == NULL || len <= 0) return;
    netlink_uevent_history_add(buf, (size_t)len);
    size_t delivered = 0;
    spin_lock(&g_netlink_lock);
    for (size_t i = 0; i < sizeof(g_netlink_sockets) / sizeof(g_netlink_sockets[0]); i++)
    {
        netlink_socket_t *sock = g_netlink_sockets[i];
        if (sock == NULL || sock->protocol != NETLINK_KOBJECT_UEVENT) continue;
        spin_lock(&sock->lock);
        if (netlink_groups_match(sock->groups, NETLINK_UEVENT_GROUP) &&
            netlink_queue_message_locked(sock, buf, (size_t)len))
        {
            delivered++;
            write_serial_fmt("[udev-trace] netlink:uevent queued sock=%p pid=%u groups=0x%x len=%d count=%llu first=%.*s\n",
                             sock,
                             sock->pid,
                             sock->groups,
                             len,
                             (unsigned long long)sock->count,
                             MIN(len, 96),
                             buf);
        }
        spin_unlock(&sock->lock);
    }
    spin_unlock(&g_netlink_lock);
    write_serial_fmt("[udev-trace] netlink:uevent-send len=%d delivered=%llu first=%.*s\n",
                     len,
                     (unsigned long long)delivered,
                     MIN(len, 96),
                     buf);
}

#include <net/unixsock.h>

extern socket_provider_t g_unix_provider;
extern void init_unix_provider(void);

int socket_provider_register(const socket_provider_t *provider)
{
    if (!socketfs_provider_valid(provider)) {
        return -EINVAL;
    }

    spin_lock(&g_socket_provider_lock);
    g_socket_provider_data  = *provider;
    g_socket_provider_ready = true;
    spin_unlock(&g_socket_provider_lock);
    return 0;
}
EXPORT_SYMBOL(socket_provider_register);

int socketfs_setup(void)
{
    socketfs_id = vfs_regist("socketfs", &socketfs_callbacks, NETFS_REGISTER_ID, 0x534f434b);
    vfs_mkdir("/sock");
    vfs_node_t node = vfs_open("/sock");
    if (node == NULL) {
        return -ENOENT;
    }
    if (vfs_mount((char *)NETFS_REGISTER_ID, node) == VFS_STATUS_FAILED) {
        return -EIO;
    }
    return 0;
}

int socket_open(int domain, int type, int protocol)
{
    if (domain == AF_NETLINK) {
        return netlink_open(type, protocol);
    }

    if (domain == AF_UNIX || domain == AF_LOCAL) {
        init_unix_provider();
        void *impl = NULL;
        int ret = unix_create(domain, type, protocol, &impl);
        if (ret < 0) return ret;
        return socketfs_alloc_fd_with_provider(impl, 0, &g_unix_provider);
    }

    if (domain != AF_INET) {
        return -EAFNOSUPPORT;
    }

    socket_provider_t provider = {};
    int               ret      = socketfs_snapshot_provider(&provider);
    if (ret < 0) {
        return ret;
    }

    printk("socketfs: open domain=%d type=%d protocol=%d create=%p\n",
           domain, type, protocol, provider.create);

    void *impl = NULL;
    ret = provider.create(domain, type, protocol, &impl);
    if (ret < 0) {
        printk("socketfs: provider create failed ret=%d\n", ret);
        return ret;
    }

    printk("socketfs: provider create ok impl=%p\n", impl);
    return socketfs_alloc_fd(impl, 0);
}

int socket_bind(int fd, const struct sockaddr *addr, socklen_t addrlen)
{
    fd_file_handle *fd_handle = NULL;
    if (!socketfs_is_socket_fd(fd, &fd_handle) || addr == NULL) {
        return -EBADF;
    }

    socketfs_handle_t *handle = socketfs_get_handle(fd_handle->node);
    if (handle == NULL || handle->provider.bind == NULL) {
        return -EBADF;
    }

    return handle->provider.bind(handle->socket, addr, addrlen);
}

int socket_connect(int fd, const struct sockaddr *addr, socklen_t addrlen)
{
    fd_file_handle *fd_handle = NULL;
    if (!socketfs_is_socket_fd(fd, &fd_handle) || addr == NULL) {
        return -EBADF;
    }

    socketfs_handle_t *handle = socketfs_get_handle(fd_handle->node);
    if (handle == NULL || handle->provider.connect == NULL) {
        return -EBADF;
    }

    return handle->provider.connect(handle->socket, addr, addrlen);
}

int socket_getsockname(int fd, struct sockaddr *addr, socklen_t *addrlen)
{
    fd_file_handle *fd_handle = NULL;
    if (!socketfs_is_socket_fd(fd, &fd_handle) || addrlen == NULL) {
        return -EBADF;
    }

    socketfs_handle_t *handle = socketfs_get_handle(fd_handle->node);
    if (handle == NULL || handle->provider.getsockname == NULL) {
        return -EBADF;
    }

    return handle->provider.getsockname(handle->socket, addr, addrlen);
}

int socket_getpeername(int fd, struct sockaddr *addr, socklen_t *addrlen)
{
    fd_file_handle *fd_handle = NULL;
    if (!socketfs_is_socket_fd(fd, &fd_handle) || addrlen == NULL) {
        return -EBADF;
    }

    socketfs_handle_t *handle = socketfs_get_handle(fd_handle->node);
    if (handle == NULL || handle->provider.getpeername == NULL) {
        return -EBADF;
    }

    return handle->provider.getpeername(handle->socket, addr, addrlen);
}

int socket_getsockopt(int fd, int level, int optname, void *optval, socklen_t *optlen)
{
    fd_file_handle *fd_handle = NULL;
    if (!socketfs_is_socket_fd(fd, &fd_handle) || optval == NULL || optlen == NULL) {
        return -EBADF;
    }

    socketfs_handle_t *handle = socketfs_get_handle(fd_handle->node);
    if (handle == NULL || handle->provider.getsockopt == NULL) {
        return -EBADF;
    }

    return handle->provider.getsockopt(handle->socket, level, optname, optval, optlen);
}

int socket_setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen)
{
    fd_file_handle *fd_handle = NULL;
    if (!socketfs_is_socket_fd(fd, &fd_handle) || (optval == NULL && optlen != 0)) {
        return -EBADF;
    }

    socketfs_handle_t *handle = socketfs_get_handle(fd_handle->node);
    if (handle == NULL || handle->provider.setsockopt == NULL) {
        return -EBADF;
    }

    return handle->provider.setsockopt(handle->socket, level, optname, optval, optlen);
}

int socket_listen(int fd, int backlog)
{
    fd_file_handle *fd_handle = NULL;
    if (!socketfs_is_socket_fd(fd, &fd_handle)) {
        return -EBADF;
    }

    socketfs_handle_t *handle = socketfs_get_handle(fd_handle->node);
    if (handle == NULL || handle->provider.listen == NULL) {
        return -EBADF;
    }

    return handle->provider.listen(handle->socket, backlog);
}

int socket_accept(int fd, struct sockaddr *addr, socklen_t *addrlen)
{
    fd_file_handle *fd_handle = NULL;
    if (!socketfs_is_socket_fd(fd, &fd_handle)) {
        return -EBADF;
    }

    socketfs_handle_t *handle = socketfs_get_handle(fd_handle->node);
    if (handle == NULL || handle->provider.accept == NULL) {
        return -EBADF;
    }

    void *new_socket = NULL;
    int   ret        = handle->provider.accept(handle->socket, &new_socket, addr, addrlen);
    if (ret < 0) {
        return ret;
    }

    int new_fd = socketfs_alloc_fd_with_provider(new_socket, fd_handle->node->flags, &handle->provider);
    if (new_fd < 0)
    {
        handle->provider.close(new_socket);
        return new_fd;
    }
    return new_fd;
}

int socket_shutdown(int fd, int how)
{
    fd_file_handle *fd_handle = NULL;
    if (!socketfs_is_socket_fd(fd, &fd_handle)) {
        return -EBADF;
    }

    socketfs_handle_t *handle = socketfs_get_handle(fd_handle->node);
    if (handle == NULL || handle->provider.shutdown == NULL) {
        return -EBADF;
    }

    return handle->provider.shutdown(handle->socket, how);
}

ssize_t socket_read(vfs_node_t node, void *buffer, size_t size)
{
    socketfs_handle_t *handle = socketfs_get_handle(node);
    if (handle == NULL || (buffer == NULL && size > 0) || handle->provider.read == NULL) {
        return -EBADF;
    }
    socketfs_sync_flags(handle, node->flags);
    return handle->provider.read(handle->socket, buffer, size, node->flags);
}

ssize_t socket_write(vfs_node_t node, const void *buffer, size_t size)
{
    socketfs_handle_t *handle = socketfs_get_handle(node);
    if (handle == NULL || (buffer == NULL && size > 0) || handle->provider.write == NULL) {
        return -EBADF;
    }
    socketfs_sync_flags(handle, node->flags);
    return handle->provider.write(handle->socket, buffer, size, node->flags);
}

ssize_t socket_recvfrom(int fd, void *buffer, size_t size, uint64_t flags, struct sockaddr *addr, socklen_t *addrlen)
{
    fd_file_handle *fd_handle = NULL;
    if (!socketfs_is_socket_fd(fd, &fd_handle)) {
        return -EBADF;
    }

    socketfs_handle_t *handle = socketfs_get_handle(fd_handle->node);
    if (handle == NULL || (buffer == NULL && size > 0) || handle->provider.recvfrom == NULL) {
        return -EBADF;
    }
    socketfs_sync_flags(handle, fd_handle->node->flags);
    return handle->provider.recvfrom(handle->socket, buffer, size, fd_handle->node->flags | flags, addr, addrlen);
}

ssize_t socket_sendto(int fd, const void *buffer, size_t size, uint64_t flags,
                      const struct sockaddr *addr, socklen_t addrlen)
{
    fd_file_handle *fd_handle = NULL;
    if (!socketfs_is_socket_fd(fd, &fd_handle)) {
        return -EBADF;
    }

    socketfs_handle_t *handle = socketfs_get_handle(fd_handle->node);
    if (handle == NULL || (buffer == NULL && size > 0) || handle->provider.sendto == NULL) {
        return -EBADF;
    }
    socketfs_sync_flags(handle, fd_handle->node->flags);
    return handle->provider.sendto(handle->socket, buffer, size, fd_handle->node->flags | flags, addr, addrlen);
}

int socket_apply_flags(vfs_node_t node, uint64_t flags)
{
    socketfs_handle_t *handle = socketfs_get_handle(node);
    if (handle == NULL || handle->provider.set_flags == NULL) {
        return -EBADF;
    }
    node->flags = flags;
    return handle->provider.set_flags(handle->socket, flags);
}

int socket_resolve_ipv4(const char *hostname, uint32_t *out_addr)
{
    socket_provider_t provider = {};
    int               ret      = socketfs_snapshot_provider(&provider);
    if (ret < 0) {
        return ret;
    }
    if (hostname == NULL || out_addr == NULL) {
        return -EINVAL;
    }
    return provider.resolve_ipv4(hostname, out_addr);
}

int socket_resolve_ipv6(const char *hostname, uint8_t out_addr[16])
{
    socket_provider_t provider = {};
    int               ret      = socketfs_snapshot_provider(&provider);
    if (ret < 0) {
        return ret;
    }
    if (hostname == NULL || out_addr == NULL) {
        return -EINVAL;
    }
    return provider.resolve_ipv6(hostname, out_addr);
}

int socket_set_dns_server(uint32_t addr)
{
    socket_provider_t provider = {};
    int               ret      = socketfs_snapshot_provider(&provider);
    if (ret < 0) {
        return ret;
    }
    return provider.set_dns_server(addr);
}

int socket_get_dns_server(uint32_t *out_addr)
{
    socket_provider_t provider = {};
    int               ret      = socketfs_snapshot_provider(&provider);
    if (ret < 0) {
        return ret;
    }
    if (out_addr == NULL) {
        return -EINVAL;
    }
    return provider.get_dns_server(out_addr);
}

int socket_set_ipv4_config(const socket_ipv4_config_t *config)
{
    socket_provider_t provider = {};
    int               ret      = socketfs_snapshot_provider(&provider);
    if (ret < 0) {
        return ret;
    }
    if (config == NULL) {
        return -EINVAL;
    }
    return provider.set_ipv4_config(config);
}

int socket_get_netinfo(socket_netinfo_t *out_info)
{
    socket_provider_t provider = {};
    int               ret      = socketfs_snapshot_provider(&provider);
    if (ret < 0) {
        return ret;
    }
    if (out_info == NULL) {
        return -EINVAL;
    }
    return provider.get_netinfo(out_info);
}
