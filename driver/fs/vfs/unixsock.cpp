#define ALL_IMPLEMENTATION
#include <net/unixsock.h>
#include <net/socket.h>
#include <errno.h>
#include <fs/vfs/vfs.h>
#include <krlibc.h>
#include <mm/heap.h>
#include <syscall/syscall.h>
#include <task/pcb.h>
#include <task/poll.h>

socket_provider_t g_unix_provider;

struct unix_path_node {
    struct unix_sock *sock;
    char             *path;
};

static int unix_pathfs_dummy()
{
    return -ENOSYS;
}

static errno_t unix_pathfs_stat(void *file, vfs_node_t node)
{
    struct unix_path_node *path_node = (struct unix_path_node *)file;
    if (path_node == NULL || node == NULL) return VFS_STATUS_FAILED;

    node->type  = file_socket;
    node->size  = 0;
    node->mode  = node->mode == 0 ? 0666 : node->mode;
    node->owner = path_node->sock != NULL ? path_node->sock->owner_uid : 0;
    node->group = path_node->sock != NULL ? path_node->sock->owner_gid : 0;
    if (node->inode == 0 && path_node->sock != NULL)
        node->inode = ((uint64_t)(uintptr_t)path_node->sock) >> 4;
    return EOK;
}

static errno_t unix_pathfs_del(void *parent, vfs_node_t node)
{
    (void)parent;
    if (node == NULL) return VFS_STATUS_FAILED;

    struct unix_path_node *path_node = (struct unix_path_node *)node->handle;
    if (path_node != NULL) {
        free(path_node->path);
        free(path_node);
        node->handle = NULL;
    }
    return EOK;
}

static struct vfs_callback unix_pathfs_callbacks = {
    .mount    = (vfs_mount_t)unix_pathfs_dummy,
    .unmount  = (vfs_unmount_t)empty,
    .open     = (vfs_open_t)empty,
    .close    = (vfs_close_t)empty,
    .read     = (vfs_read_t)unix_pathfs_dummy,
    .write    = (vfs_write_t)unix_pathfs_dummy,
    .readlink = (vfs_readlink_t)unix_pathfs_dummy,
    .mkdir    = (vfs_mk_t)unix_pathfs_dummy,
    .mkfile   = (vfs_mk_t)unix_pathfs_dummy,
    .link     = (vfs_mk_t)unix_pathfs_dummy,
    .symlink  = (vfs_mk_t)unix_pathfs_dummy,
    .stat     = unix_pathfs_stat,
    .ioctl    = (vfs_ioctl_t)unix_pathfs_dummy,
    .dup      = (vfs_dup_t)empty,
    .poll     = (vfs_poll_t)unix_pathfs_dummy,
    .map      = (vfs_mapfile_t)empty,
    .resize   = (vfs_resize_t)unix_pathfs_dummy,
    .del      = unix_pathfs_del,
    .rename   = (vfs_rename_t)unix_pathfs_dummy,
};

static int unix_pathfs_id = 0;

void init_unix_provider(void)
{
    static bool inited = false;
    if (inited) return;
    inited = true;
    memset(&g_unix_provider, 0, sizeof(g_unix_provider));
    g_unix_provider.create       = unix_create;
    g_unix_provider.close        = unix_close;
    g_unix_provider.read         = unix_read;
    g_unix_provider.write        = unix_write;
    g_unix_provider.bind         = unix_bind;
    g_unix_provider.recvfrom     = unix_recvfrom;
    g_unix_provider.sendto       = unix_sendto;
    g_unix_provider.getsockname  = unix_getsockname;
    g_unix_provider.getpeername  = unix_getpeername;
    g_unix_provider.getsockopt   = unix_getsockopt;
    g_unix_provider.setsockopt   = unix_setsockopt;
    g_unix_provider.connect      = unix_connect;
    g_unix_provider.listen       = unix_listen;
    g_unix_provider.accept       = unix_accept;
    g_unix_provider.shutdown     = unix_shutdown;
    g_unix_provider.poll         = unix_poll;
    g_unix_provider.set_flags    = unix_set_flags;
    unix_pathfs_id               = vfs_regist("unixpath", &unix_pathfs_callbacks, 0, 0x554e4958);
}

static lock_queue *g_unix_namespace = NULL;
static spin_t      g_unix_ns_lock   = SPIN_INIT;

static size_t unix_bounded_strlen(const char *str, size_t max_len)
{
    if (str == NULL) return 0;
    size_t len = 0;
    while (len < max_len && str[len] != '\0')
        len++;
    return len;
}

static char *unix_bound_path_dup(const struct sockaddr_un *addr, socklen_t addrlen)
{
    if (addr == NULL || addr->sun_path[0] == '\0') return NULL;

    size_t path_len = 0;
    if (addrlen > (socklen_t)offsetof(struct sockaddr_un, sun_path)) {
        path_len = (size_t)addrlen - offsetof(struct sockaddr_un, sun_path);
        if (path_len > UNIX_PATH_MAX) path_len = UNIX_PATH_MAX;
    } else {
        path_len = unix_bounded_strlen(addr->sun_path, UNIX_PATH_MAX);
    }

    if (path_len == 0) return NULL;

    size_t actual_len = unix_bounded_strlen(addr->sun_path, path_len);
    if (actual_len == 0 || actual_len >= UNIX_PATH_MAX) return NULL;

    char *path = (char *)malloc(actual_len + 1);
    if (path == NULL) return NULL;
    memcpy(path, addr->sun_path, actual_len);
    path[actual_len] = '\0';
    return path;
}

static vfs_node_t unix_path_parent_open(const char *path, char **out_name)
{
    if (path == NULL || path[0] != '/' || out_name == NULL) return NULL;
    *out_name = NULL;

    char *fullpath = strdup(path);
    if (fullpath == NULL) return NULL;

    char *filename = strrchr(fullpath, '/');
    if (filename == NULL || filename[1] == '\0') {
        free(fullpath);
        return NULL;
    }

    *filename++ = '\0';
    char *name = strdup(filename);
    if (name == NULL) {
        free(fullpath);
        return NULL;
    }

    vfs_node_t parent = fullpath[0] == '\0' ? get_rootdir() : vfs_open(fullpath);
    free(fullpath);
    if (parent == NULL || !(parent->type & file_dir)) {
        if (parent != NULL && parent != get_rootdir()) vfs_close(parent);
        free(name);
        return NULL;
    }

    *out_name = name;
    return parent;
}

static int unix_create_path_node(struct unix_sock *sk, const char *path)
{
    if (sk == NULL || path == NULL || path[0] != '/' || unix_pathfs_id == 0) return -EINVAL;

    vfs_node_t existing = vfs_open_no_follow(path);
    if (existing != NULL) {
        vfs_close(existing);
        return -EADDRINUSE;
    }

    char *name = NULL;
    vfs_node_t parent = unix_path_parent_open(path, &name);
    if (parent == NULL || name == NULL) {
        free(name);
        return -ENOENT;
    }

    vfs_node_t node = vfs_child_append(parent, name, NULL);
    free(name);
    if (node == NULL) {
        if (parent != get_rootdir()) vfs_close(parent);
        return -ENOMEM;
    }

    struct unix_path_node *path_node = (struct unix_path_node *)calloc(1, sizeof(struct unix_path_node));
    if (path_node == NULL) {
        vfs_child_lock();
        parent->child = list_delete(parent->child, node);
        vfs_child_unlock();
        free(node->name);
        free(node);
        if (parent != get_rootdir()) vfs_close(parent);
        return -ENOMEM;
    }

    path_node->sock = sk;
    path_node->path = strdup(path);
    if (path_node->path == NULL) {
        free(path_node);
        vfs_child_lock();
        parent->child = list_delete(parent->child, node);
        vfs_child_unlock();
        free(node->name);
        free(node);
        if (parent != get_rootdir()) vfs_close(parent);
        return -ENOMEM;
    }

    node->handle = path_node;
    node->type  = file_socket;
    node->fsid  = unix_pathfs_id;
    node->size  = 0;
    node->flags = 0;
    node->mode  = 0666;
    node->owner = sk->owner_uid;
    node->group = sk->owner_gid;
    node->inode = ((uint64_t)(uintptr_t)sk) >> 4;
    node->visited = true;

    if (parent != get_rootdir()) vfs_close(parent);
    return 0;
}

static void unix_delete_path_node(const char *path)
{
    if (path == NULL || path[0] != '/') return;

    vfs_node_t node = vfs_open_no_follow(path);
    if (node == NULL) return;
    if (node->type & file_socket)
        (void)vfs_delete(node);
    else
        vfs_close(node);
}

static void unix_sock_init(struct unix_sock *sk, int type, int protocol)
{
    tcb_t task = get_current_task();
    sk->state    = UNIX_FREE;
    sk->type     = type;
    sk->protocol = protocol;
    sk->owner_pid = task != NULL && task->parent_group != NULL ? (int)task->parent_group->pid : 0;
    sk->owner_uid = user_uid(task_effective_user());
    sk->owner_gid = user_gid(task_effective_user());
    sk->peer     = NULL;
    sk->conn_queue = NULL;
    sk->flags    = 0;
    sk->in_cap = UNIX_RING_SIZE;
    sk->in_buf = (uint8_t *)malloc(UNIX_RING_SIZE);
    sk->in_head = sk->in_tail = 0;
    sk->dg_queue = NULL;
    sk->stream_queue = NULL;
    sk->lock     = SPIN_INIT;
    sk->refcount = 1;
    sk->closed   = false;
    sk->namespace_registered = false;
    sk->path_node_created    = false;
    sk->passcred             = false;
    sk->reuseaddr            = false;
    sk->reuseport            = false;
    sk->keepalive            = false;
    sk->broadcast            = false;
    sk->passsec              = false;
    sk->timestamp_legacy     = false;
    sk->timestampns_legacy   = false;
    sk->timestamping_legacy  = false;
    sk->recv_buffer_size     = UNIX_RING_SIZE;
    sk->send_buffer_size     = UNIX_RING_SIZE;
    sk->recv_lowat           = 1;
    sk->send_lowat           = 1;
    sk->priority             = 0;
    sk->peek_off             = -1;
    memset(&sk->recv_timeout, 0, sizeof(sk->recv_timeout));
    memset(&sk->send_timeout, 0, sizeof(sk->send_timeout));
    memset(&sk->linger_opt, 0, sizeof(sk->linger_opt));
    memset(&sk->local_addr, 0, sizeof(sk->local_addr));
    memset(&sk->peer_addr, 0, sizeof(sk->peer_addr));
}

static struct ucred unix_current_cred(void)
{
    tcb_t task = get_current_task();
    struct ucred cred = {};
    cred.pid = task != NULL && task->parent_group != NULL ? (int)task->parent_group->pid : 0;
    cred.uid = (int)user_uid(task_effective_user());
    cred.gid = (int)user_gid(task_effective_user());
    return cred;
}

extern fd_file_handle *fd_dup(fd_file_handle *src);

static void unix_release_fd_array(fd_file_handle **fds, size_t fd_count)
{
    if (fds == NULL) return;
    for (size_t i = 0; i < fd_count; i++) {
        if (fds[i] == NULL) continue;
        vfs_close(fds[i]->node);
        free(fds[i]);
    }
    free(fds);
}

static fd_file_handle **unix_copy_fd_array(fd_file_handle **fds, size_t fd_count)
{
    if (fds == NULL || fd_count == 0) return NULL;

    fd_file_handle **out = (fd_file_handle **)calloc(fd_count, sizeof(fd_file_handle *));
    if (out == NULL) return NULL;

    for (size_t i = 0; i < fd_count; i++) {
        if (fds[i] == NULL) continue;

        out[i] = fd_dup(fds[i]);
        if (out[i] == NULL) {
            unix_release_fd_array(out, fd_count);
            return NULL;
        }
        out[i]->fd = (size_t)-1;
    }

    return out;
}

static void unix_stream_packet_free(struct unix_stream_packet *packet)
{
    if (packet == NULL) return;
    unix_release_fd_array(packet->fds, packet->fd_count);
    free(packet);
}

static void unix_dgram_free(struct unix_dgram *dg)
{
    if (dg == NULL) return;
    unix_release_fd_array(dg->fds, dg->fd_count);
    free(dg);
}

static void unix_sock_free(struct unix_sock *sk)
{
    if (sk == NULL) return;
    if (sk->in_buf) { free(sk->in_buf); sk->in_buf = NULL; }
    if (sk->conn_queue) {
        while (sk->conn_queue->head) {
            struct unix_sock *cs = (struct unix_sock *)sk->conn_queue->head->data;
            if (cs) {
                spin_lock(&cs->lock);
                if (cs->peer != NULL) {
                    spin_lock(&cs->peer->lock);
                    cs->peer->peer = NULL;
                    cs->peer->closed = true;
                    spin_unlock(&cs->peer->lock);
                }
                cs->peer = NULL;
                cs->state = UNIX_FREE;
                spin_unlock(&cs->lock);
                unix_sock_free(cs);
            }
            queue_remove_node(sk->conn_queue, sk->conn_queue->head);
        }
        free(sk->conn_queue);
    }
    if (sk->dg_queue) {
        while (sk->dg_queue->head) {
            struct unix_dgram *dg = (struct unix_dgram *)sk->dg_queue->head->data;
            queue_remove_node(sk->dg_queue, sk->dg_queue->head);
            unix_dgram_free(dg);
        }
        free(sk->dg_queue);
    }
    if (sk->stream_queue) {
        while (sk->stream_queue->head) {
            struct unix_stream_packet *packet = (struct unix_stream_packet *)sk->stream_queue->head->data;
            queue_remove_node(sk->stream_queue, sk->stream_queue->head);
            unix_stream_packet_free(packet);
        }
        free(sk->stream_queue);
    }
    free(sk);
}

static void unix_ns_register(struct unix_sock *sk)
{
    if (sk->local_addr.sun_path[0] == '\0') return;
    spin_lock(&g_unix_ns_lock);
    if (g_unix_namespace == NULL)
        g_unix_namespace = queue_init();
    if (g_unix_namespace != NULL) {
        queue_enqueue(g_unix_namespace, sk);
        sk->namespace_registered = true;
    }
    spin_unlock(&g_unix_ns_lock);
}

static void unix_ns_unregister(struct unix_sock *sk)
{
    if (sk == NULL || !sk->namespace_registered || g_unix_namespace == NULL) return;
    spin_lock(&g_unix_ns_lock);
    lock_node *node = g_unix_namespace->head;
    while (node != NULL) {
        if (node->data == sk) {
            queue_remove_node(g_unix_namespace, node);
            sk->namespace_registered = false;
            break;
        }
        node = node->next;
    }
    spin_unlock(&g_unix_ns_lock);
}

static struct unix_sock *unix_ns_lookup(const char *path)
{
    if (g_unix_namespace == NULL || path == NULL) return NULL;
    spin_lock(&g_unix_ns_lock);
    lock_node *node = g_unix_namespace->head;
    while (node != NULL) {
        struct unix_sock *sk = (struct unix_sock *)node->data;
        if (sk != NULL && sk->state >= UNIX_BOUND &&
            sk->local_addr.sun_path[0] != '\0' &&
            !strcmp(sk->local_addr.sun_path, path)) {
            sk->refcount++;
            spin_unlock(&g_unix_ns_lock);
            return sk;
        }
        node = node->next;
    }
    spin_unlock(&g_unix_ns_lock);
    return NULL;
}

static size_t ring_readable(struct unix_sock *sk)
{
    if (sk->in_head >= sk->in_tail) return sk->in_head - sk->in_tail;
    return sk->in_cap - sk->in_tail + sk->in_head;
}

static size_t ring_writable(struct unix_sock *sk)
{
    size_t used = ring_readable(sk);
    size_t free_space = sk->in_cap - used;
    return free_space > 1 ? free_space - 1 : 0;
}

static size_t ring_write(struct unix_sock *sk, const uint8_t *data, size_t len)
{
    size_t avail = ring_writable(sk);
    if (avail == 0 || len == 0) return 0;
    size_t n = len < avail ? len : avail;
    for (size_t i = 0; i < n; i++) {
        sk->in_buf[sk->in_head] = data[i];
        sk->in_head = (sk->in_head + 1) % sk->in_cap;
    }
    return n;
}

static size_t ring_read(struct unix_sock *sk, uint8_t *data, size_t len)
{
    size_t avail = ring_readable(sk);
    if (avail == 0 || len == 0) return 0;
    size_t n = len < avail ? len : avail;
    for (size_t i = 0; i < n; i++) {
        data[i] = sk->in_buf[sk->in_tail];
        sk->in_tail = (sk->in_tail + 1) % sk->in_cap;
    }
    return n;
}

static struct unix_stream_packet *unix_stream_packet_alloc(const uint8_t *data, size_t len,
                                                           fd_file_handle **fds, size_t fd_count,
                                                           const struct ucred *cred, bool has_cred)
{
    struct unix_stream_packet *packet =
        (struct unix_stream_packet *)malloc(sizeof(struct unix_stream_packet) + len);
    if (packet == NULL) return NULL;

    packet->len      = len;
    packet->offset   = 0;
    packet->fd_count = fd_count;
    packet->fds      = fds;
    packet->cred     = has_cred && cred != NULL ? *cred : unix_current_cred();
    packet->has_cred = true;
    if (len > 0 && data != NULL) {
        memcpy(packet->data, data, len);
    }
    return packet;
}

static ssize_t unix_stream_enqueue_locked(struct unix_sock *peer, const uint8_t *data, size_t len,
                                          fd_file_handle **fds, size_t fd_count,
                                          const struct ucred *cred, bool has_cred)
{
    if (peer == NULL) return -EPIPE;
    if (len == 0 && fd_count == 0 && !has_cred) return 0;

    if (peer->stream_queue == NULL) {
        peer->stream_queue = queue_init();
        if (peer->stream_queue == NULL) return -ENOMEM;
    }

    struct unix_stream_packet *packet = unix_stream_packet_alloc(data, len, fds, fd_count, cred, has_cred);
    if (packet == NULL) return -ENOMEM;
    if (queue_enqueue(peer->stream_queue, packet) == (size_t)-1) {
        packet->fds = NULL;
        unix_stream_packet_free(packet);
        return -ENOMEM;
    }
    poll_notify_event();
    return (ssize_t)len;
}

static struct unix_dgram *unix_dgram_alloc(const struct sockaddr_un *addr, const uint8_t *data, size_t len,
                                           fd_file_handle **fds, size_t fd_count,
                                           const struct ucred *cred, bool has_cred)
{
    struct unix_dgram *dg = (struct unix_dgram *)malloc(sizeof(struct unix_dgram) + len);
    if (dg == NULL) return NULL;

    if (addr != NULL)
        memcpy(&dg->addr, addr, sizeof(dg->addr));
    else
        memset(&dg->addr, 0, sizeof(dg->addr));
    dg->addr.sun_family = AF_UNIX;
    dg->cred = has_cred && cred != NULL ? *cred : unix_current_cred();
    dg->has_cred = true;
    dg->fd_count = fd_count;
    dg->fds = fds;
    dg->len = len;
    if (len > 0 && data != NULL) {
        memcpy(dg->data, data, len);
    }
    return dg;
}

static ssize_t unix_dgram_enqueue_locked(struct unix_sock *peer, const struct sockaddr_un *addr,
                                         const uint8_t *data, size_t len,
                                         fd_file_handle **fds, size_t fd_count,
                                         const struct ucred *cred, bool has_cred)
{
    if (peer == NULL) {
        unix_release_fd_array(fds, fd_count);
        return -EPIPE;
    }
    if (peer->closed) {
        unix_release_fd_array(fds, fd_count);
        return -EPIPE;
    }
    if (peer->dg_queue == NULL) {
        peer->dg_queue = queue_init();
        if (peer->dg_queue == NULL) {
            unix_release_fd_array(fds, fd_count);
            return -ENOMEM;
        }
    }

    struct unix_dgram *dg = unix_dgram_alloc(addr, data, len, fds, fd_count, cred, has_cred);
    if (dg == NULL) {
        unix_release_fd_array(fds, fd_count);
        return -ENOMEM;
    }
    if (queue_enqueue(peer->dg_queue, dg) == (size_t)-1) {
        dg->fds = NULL;
        unix_dgram_free(dg);
        unix_release_fd_array(fds, fd_count);
        return -ENOMEM;
    }
    poll_notify_event();
    return (ssize_t)len;
}

static ssize_t unix_dgram_send_to_peer(struct unix_sock *sk, struct unix_sock *peer,
                                       const uint8_t *data, size_t len,
                                       fd_file_handle **fds, size_t fd_count,
                                       const struct ucred *cred, bool has_cred)
{
    if (peer == NULL) {
        unix_release_fd_array(fds, fd_count);
        return -EPIPE;
    }

    struct sockaddr_un from = {};
    if (sk != NULL && sk->local_addr.sun_family == AF_UNIX)
        memcpy(&from, &sk->local_addr, sizeof(from));
    from.sun_family = AF_UNIX;

    spin_lock(&peer->lock);
    ssize_t ret = unix_dgram_enqueue_locked(peer, &from, data, len, fds, fd_count, cred, has_cred);
    spin_unlock(&peer->lock);
    return ret;
}

static ssize_t unix_dgram_send_to_path(struct unix_sock *sk, const char *path,
                                       const uint8_t *data, size_t len,
                                       fd_file_handle **fds, size_t fd_count,
                                       const struct ucred *cred, bool has_cred)
{
    if (path == NULL || path[0] == '\0') {
        unix_release_fd_array(fds, fd_count);
        return -EDESTADDRREQ;
    }

    struct unix_sock *peer = unix_ns_lookup(path);
    if (peer == NULL) {
        unix_release_fd_array(fds, fd_count);
        return -ECONNREFUSED;
    }

    ssize_t ret = unix_dgram_send_to_peer(sk, peer, data, len, fds, fd_count, cred, has_cred);
    peer->refcount--;
    return ret;
}

int unix_create(int domain, int type, int protocol, void **out_impl)
{
    (void)domain;
    if (out_impl == NULL) return -EINVAL;
    if (type != SOCK_STREAM && type != SOCK_DGRAM && type != SOCK_SEQPACKET) return -EPROTONOSUPPORT;

    struct unix_sock *sk = (struct unix_sock *)malloc(sizeof(struct unix_sock));
    if (sk == NULL) return -ENOMEM;
    unix_sock_init(sk, type, protocol);
    *out_impl = sk;
    return 0;
}

int unix_bind(void *impl, const struct sockaddr *addr, socklen_t addrlen)
{
    struct unix_sock *sk = (struct unix_sock *)impl;
    if (sk == NULL || addr == NULL) return -EINVAL;
    if (addrlen < (socklen_t)sizeof(sa_family_t)) return -EINVAL;

    const struct sockaddr_un *sunaddr = (const struct sockaddr_un *)addr;
    if (sunaddr->sun_family != AF_UNIX) return -EAFNOSUPPORT;

    spin_lock(&sk->lock);
    if (sk->state != UNIX_FREE) { spin_unlock(&sk->lock); return -EINVAL; }

    size_t max_path = 0;
    if (addrlen > (socklen_t)offsetof(struct sockaddr_un, sun_path)) {
        max_path = (size_t)addrlen - offsetof(struct sockaddr_un, sun_path);
        if (max_path > UNIX_PATH_MAX) max_path = UNIX_PATH_MAX;
    }

    char *bound_path = NULL;
    if (max_path > 0 && sunaddr->sun_path[0] != '\0') {
        bound_path = unix_bound_path_dup(sunaddr, addrlen);
        if (bound_path == NULL) {
            spin_unlock(&sk->lock);
            return -EINVAL;
        }
        struct unix_sock *existing = unix_ns_lookup(bound_path);
        if (existing != NULL) {
            existing->refcount--;
            free(bound_path);
            spin_unlock(&sk->lock);
            return -EADDRINUSE;
        }
        int node_ret = unix_create_path_node(sk, bound_path);
        if (node_ret < 0) {
            free(bound_path);
            spin_unlock(&sk->lock);
            return node_ret;
        }
    }

    memset(&sk->local_addr, 0, sizeof(sk->local_addr));
    memcpy(&sk->local_addr, sunaddr, addrlen < sizeof(sk->local_addr) ? (size_t)addrlen : sizeof(sk->local_addr));
    if (bound_path != NULL)
        strncpy(sk->local_addr.sun_path, bound_path, sizeof(sk->local_addr.sun_path) - 1);
    sk->state = UNIX_BOUND;

    if (bound_path != NULL) {
        sk->path_node_created = true;
        unix_ns_register(sk);
    }

    spin_unlock(&sk->lock);
    free(bound_path);
    return 0;
}

int unix_connect(void *impl, const struct sockaddr *addr, socklen_t addrlen)
{
    struct unix_sock *sk = (struct unix_sock *)impl;
    if (sk == NULL || addr == NULL) return -EINVAL;

    const struct sockaddr_un *sunaddr = (const struct sockaddr_un *)addr;
    if (sunaddr->sun_family != AF_UNIX) return -EAFNOSUPPORT;

    spin_lock(&sk->lock);
    if (sk->state != UNIX_FREE && sk->state != UNIX_BOUND) {
        spin_unlock(&sk->lock);
        return -EINVAL;
    }

    if (sk->local_addr.sun_family != AF_UNIX)
        sk->local_addr.sun_family = AF_UNIX;

    char *peer_path = NULL;
    if (sunaddr->sun_path[0] != '\0') {
        peer_path = unix_bound_path_dup(sunaddr, addrlen);
        if (peer_path == NULL) {
            spin_unlock(&sk->lock);
            return -EINVAL;
        }
    }

    memset(&sk->peer_addr, 0, sizeof(sk->peer_addr));
    memcpy(&sk->peer_addr, sunaddr, addrlen < sizeof(sk->peer_addr) ? (size_t)addrlen : sizeof(sk->peer_addr));
    if (peer_path != NULL)
        strncpy(sk->peer_addr.sun_path, peer_path, sizeof(sk->peer_addr.sun_path) - 1);

    if (sk->type == SOCK_DGRAM) {
        sk->state = UNIX_CONNECTED;
        spin_unlock(&sk->lock);
        free(peer_path);
        return 0;
    }

    struct unix_sock *listener = NULL;
    if (peer_path != NULL)
        listener = unix_ns_lookup(peer_path);

    if (listener == NULL) {
        free(peer_path);
        spin_unlock(&sk->lock);
        return -ECONNREFUSED;
    }

    spin_lock(&listener->lock);
    if (listener->state != UNIX_LISTENING || listener->conn_queue == NULL) {
        spin_unlock(&listener->lock);
        listener->refcount--;
        free(peer_path);
        spin_unlock(&sk->lock);
        return -ECONNREFUSED;
    }

    struct unix_sock *server_sk = (struct unix_sock *)malloc(sizeof(struct unix_sock));
    if (server_sk == NULL) {
        spin_unlock(&listener->lock);
        listener->refcount--;
        free(peer_path);
        spin_unlock(&sk->lock);
        return -ENOMEM;
    }
    unix_sock_init(server_sk, listener->type, sk->protocol);
    server_sk->state = UNIX_CONNECTED;
    server_sk->owner_pid = listener->owner_pid;
    server_sk->owner_uid = listener->owner_uid;
    server_sk->owner_gid = listener->owner_gid;
    server_sk->local_addr = listener->local_addr;
    server_sk->peer_addr  = sk->local_addr;
    server_sk->peer       = sk;
    server_sk->passcred   = listener->passcred;

    sk->state = UNIX_CONNECTED;
    sk->peer  = server_sk;

    if (queue_enqueue(listener->conn_queue, server_sk) == (size_t)-1) {
        sk->peer = NULL;
        sk->state = sk->local_addr.sun_family == AF_UNIX ? UNIX_BOUND : UNIX_FREE;
        unix_sock_free(server_sk);
        spin_unlock(&listener->lock);
        listener->refcount--;
        free(peer_path);
        spin_unlock(&sk->lock);
        return -ENOMEM;
    }
    poll_notify_event();
    spin_unlock(&listener->lock);
    spin_unlock(&sk->lock);
    free(peer_path);
    return 0;
}

int unix_listen(void *impl, int backlog)
{
    struct unix_sock *sk = (struct unix_sock *)impl;
    if (sk == NULL) return -EINVAL;

    spin_lock(&sk->lock);
    if (sk->type != SOCK_STREAM && sk->type != SOCK_SEQPACKET) { spin_unlock(&sk->lock); return -EOPNOTSUPP; }
    if (sk->state != UNIX_BOUND) { spin_unlock(&sk->lock); return -EINVAL; }

    sk->state = UNIX_LISTENING;
    if (sk->conn_queue == NULL)
        sk->conn_queue = queue_init();
    (void)backlog;
    spin_unlock(&sk->lock);
    return 0;
}

int unix_accept(void *impl, void **out_new, struct sockaddr *addr, socklen_t *addrlen)
{
    struct unix_sock *sk = (struct unix_sock *)impl;
    if (sk == NULL || out_new == NULL) return -EINVAL;

    spin_lock(&sk->lock);
    if (sk->state != UNIX_LISTENING || sk->conn_queue == NULL) {
        spin_unlock(&sk->lock);
        return -EINVAL;
    }

    while (sk->conn_queue->head == NULL) {
        if (sk->flags & O_NONBLOCK) {
            spin_unlock(&sk->lock);
            return -EAGAIN;
        }
        spin_unlock(&sk->lock);
        scheduler_yield();
        spin_lock(&sk->lock);
        if (sk->state != UNIX_LISTENING) { spin_unlock(&sk->lock); return -EINVAL; }
    }

    struct unix_sock *new_sk = (struct unix_sock *)sk->conn_queue->head->data;
    queue_remove_node(sk->conn_queue, sk->conn_queue->head);
    struct unix_sock *client = new_sk->peer;
    if (client != NULL) {
        spin_lock(&client->lock);
        client->state = UNIX_CONNECTED;
        client->peer_addr = new_sk->local_addr;
        spin_unlock(&client->lock);
    }

    if (addr != NULL && addrlen != NULL) {
        socklen_t copy_len = client != NULL ? sizeof(new_sk->peer_addr) : 0;
        if (*addrlen < copy_len) copy_len = *addrlen;
        if (client != NULL) memcpy(addr, &new_sk->peer_addr, copy_len);
        *addrlen = client != NULL ? sizeof(new_sk->peer_addr) : 0;
    }

    *out_new = new_sk;
    spin_unlock(&sk->lock);
    return 0;
}

int unix_getsockname(void *impl, struct sockaddr *addr, socklen_t *addrlen)
{
    struct unix_sock *sk = (struct unix_sock *)impl;
    if (sk == NULL || addr == NULL || addrlen == NULL) return -EINVAL;
    socklen_t copy = *addrlen < (socklen_t)sizeof(sk->local_addr) ? *addrlen : (socklen_t)sizeof(sk->local_addr);
    memcpy(addr, &sk->local_addr, copy);
    *addrlen = sizeof(sk->local_addr);
    return 0;
}

int unix_getpeername(void *impl, struct sockaddr *addr, socklen_t *addrlen)
{
    struct unix_sock *sk = (struct unix_sock *)impl;
    if (sk == NULL || addr == NULL || addrlen == NULL) return -EINVAL;
    if (sk->peer_addr.sun_family != AF_UNIX) return -ENOTCONN;
    socklen_t copy = *addrlen < (socklen_t)sizeof(sk->peer_addr) ? *addrlen : (socklen_t)sizeof(sk->peer_addr);
    memcpy(addr, &sk->peer_addr, copy);
    *addrlen = sizeof(sk->peer_addr);
    return 0;
}

static int unix_copy_sockopt(void *optval, socklen_t *optlen, const void *src, socklen_t len)
{
    if (optval == NULL || optlen == NULL || src == NULL) return -EINVAL;
    if (*optlen < len) return -EINVAL;
    memcpy(optval, src, len);
    *optlen = len;
    return 0;
}

static int unix_getsockopt_int(void *optval, socklen_t *optlen, int value)
{
    return unix_copy_sockopt(optval, optlen, &value, (socklen_t)sizeof(value));
}

static int unix_read_sockopt_int(const void *optval, socklen_t optlen, int *out_value)
{
    if (optval == NULL || out_value == NULL || optlen < (socklen_t)sizeof(int)) return -EINVAL;
    memcpy(out_value, optval, sizeof(int));
    return 0;
}

static bool unix_sock_has_input_locked(struct unix_sock *sk)
{
    if (sk == NULL) return false;
    if (sk->type == SOCK_DGRAM)
        return sk->dg_queue != NULL && sk->dg_queue->head != NULL;
    return sk->stream_queue != NULL && sk->stream_queue->head != NULL;
}

int unix_getsockopt(void *impl, int level, int optname, void *optval, socklen_t *optlen)
{
    struct unix_sock *sk = (struct unix_sock *)impl;
    if (sk == NULL || optval == NULL || optlen == NULL) return -EINVAL;

    if (level == SOL_SOCKET) {
        switch (optname) {
        case SO_TYPE:
            return unix_getsockopt_int(optval, optlen, sk->type);
        case SO_ERROR:
            return unix_getsockopt_int(optval, optlen, 0);
        case SO_ACCEPTCONN:
            return unix_getsockopt_int(optval, optlen, sk->state == UNIX_LISTENING ? 1 : 0);
        case SO_PROTOCOL:
            return unix_getsockopt_int(optval, optlen, sk->protocol);
        case SO_DOMAIN:
            return unix_getsockopt_int(optval, optlen, AF_UNIX);
        case SO_SNDBUF:
        case SO_SNDBUFFORCE:
            return unix_getsockopt_int(optval, optlen, sk->send_buffer_size);
        case SO_RCVBUF:
        case SO_RCVBUFFORCE:
            return unix_getsockopt_int(optval, optlen, sk->recv_buffer_size);
        case SO_SNDLOWAT:
            return unix_getsockopt_int(optval, optlen, sk->send_lowat);
        case SO_RCVLOWAT:
            return unix_getsockopt_int(optval, optlen, sk->recv_lowat);
        case SO_REUSEADDR:
            return unix_getsockopt_int(optval, optlen, sk->reuseaddr ? 1 : 0);
        case SO_REUSEPORT:
            return unix_getsockopt_int(optval, optlen, sk->reuseport ? 1 : 0);
        case SO_KEEPALIVE:
            return unix_getsockopt_int(optval, optlen, sk->keepalive ? 1 : 0);
        case SO_BROADCAST:
            return unix_getsockopt_int(optval, optlen, sk->broadcast ? 1 : 0);
        case SO_PASSCRED:
            return unix_getsockopt_int(optval, optlen, sk->passcred ? 1 : 0);
        case SO_PASSSEC:
            return unix_getsockopt_int(optval, optlen, sk->passsec ? 1 : 0);
        case SO_PRIORITY:
            return unix_getsockopt_int(optval, optlen, sk->priority);
        case SO_PEEK_OFF:
            return unix_getsockopt_int(optval, optlen, sk->peek_off);
        case SO_TIMESTAMP_OLD:
        case SO_TIMESTAMP_NEW:
            return unix_getsockopt_int(optval, optlen, sk->timestamp_legacy ? 1 : 0);
        case SO_TIMESTAMPNS_OLD:
        case SO_TIMESTAMPNS_NEW:
            return unix_getsockopt_int(optval, optlen, sk->timestampns_legacy ? 1 : 0);
        case SO_TIMESTAMPING_OLD:
        case SO_TIMESTAMPING_NEW:
            return unix_getsockopt_int(optval, optlen, sk->timestamping_legacy ? 1 : 0);
        case SO_SNDTIMEO_OLD:
        case SO_SNDTIMEO_NEW:
            return unix_copy_sockopt(optval, optlen, &sk->send_timeout, (socklen_t)sizeof(sk->send_timeout));
        case SO_RCVTIMEO_OLD:
        case SO_RCVTIMEO_NEW:
            return unix_copy_sockopt(optval, optlen, &sk->recv_timeout, (socklen_t)sizeof(sk->recv_timeout));
        case SO_LINGER:
            return unix_copy_sockopt(optval, optlen, &sk->linger_opt, (socklen_t)sizeof(sk->linger_opt));
        case SO_PEERCRED: {
            struct ucred cred = {};
            spin_lock(&sk->lock);
            if (sk->peer != NULL) {
                cred.pid = sk->peer->owner_pid;
                cred.uid = (int)sk->peer->owner_uid;
                cred.gid = (int)sk->peer->owner_gid;
            } else {
                cred.pid = sk->owner_pid;
                cred.uid = (int)sk->owner_uid;
                cred.gid = (int)sk->owner_gid;
            }
            spin_unlock(&sk->lock);
            return unix_copy_sockopt(optval, optlen, &cred, (socklen_t)sizeof(cred));
        }
        case SO_PEERSEC:
        case SO_PEERGROUPS:
        case SO_PEERPIDFD:
            return -ENOPROTOOPT;
        default: return -ENOPROTOOPT;
        }
    }
    return -ENOPROTOOPT;
}

int unix_setsockopt(void *impl, int level, int optname, const void *optval, socklen_t optlen)
{
    struct unix_sock *sk = (struct unix_sock *)impl;
    if (sk == NULL) return -EINVAL;
    if (level != SOL_SOCKET) return -ENOPROTOOPT;

    int int_value = 0;
    switch (optname) {
    case SO_PASSCRED:
        if (unix_read_sockopt_int(optval, optlen, &int_value) < 0) return -EINVAL;
        spin_lock(&sk->lock);
        sk->passcred = int_value != 0;
        spin_unlock(&sk->lock);
        return 0;
    case SO_REUSEADDR:
        if (unix_read_sockopt_int(optval, optlen, &int_value) < 0) return -EINVAL;
        spin_lock(&sk->lock);
        sk->reuseaddr = int_value != 0;
        spin_unlock(&sk->lock);
        return 0;
    case SO_REUSEPORT:
        if (unix_read_sockopt_int(optval, optlen, &int_value) < 0) return -EINVAL;
        spin_lock(&sk->lock);
        sk->reuseport = int_value != 0;
        spin_unlock(&sk->lock);
        return 0;
    case SO_KEEPALIVE:
        if (unix_read_sockopt_int(optval, optlen, &int_value) < 0) return -EINVAL;
        spin_lock(&sk->lock);
        sk->keepalive = int_value != 0;
        spin_unlock(&sk->lock);
        return 0;
    case SO_BROADCAST:
        if (unix_read_sockopt_int(optval, optlen, &int_value) < 0) return -EINVAL;
        spin_lock(&sk->lock);
        sk->broadcast = int_value != 0;
        spin_unlock(&sk->lock);
        return 0;
    case SO_PASSSEC:
        if (unix_read_sockopt_int(optval, optlen, &int_value) < 0) return -EINVAL;
        spin_lock(&sk->lock);
        sk->passsec = int_value != 0;
        spin_unlock(&sk->lock);
        return 0;
    case SO_SNDBUF:
    case SO_SNDBUFFORCE:
        if (unix_read_sockopt_int(optval, optlen, &int_value) < 0) return -EINVAL;
        if (int_value < UNIX_RING_SIZE) int_value = UNIX_RING_SIZE;
        spin_lock(&sk->lock);
        sk->send_buffer_size = int_value;
        spin_unlock(&sk->lock);
        return 0;
    case SO_RCVBUF:
    case SO_RCVBUFFORCE:
        if (unix_read_sockopt_int(optval, optlen, &int_value) < 0) return -EINVAL;
        if (int_value < UNIX_RING_SIZE) int_value = UNIX_RING_SIZE;
        spin_lock(&sk->lock);
        sk->recv_buffer_size = int_value;
        spin_unlock(&sk->lock);
        return 0;
    case SO_SNDLOWAT:
        if (unix_read_sockopt_int(optval, optlen, &int_value) < 0) return -EINVAL;
        if (int_value <= 0) return -EINVAL;
        spin_lock(&sk->lock);
        sk->send_lowat = int_value;
        spin_unlock(&sk->lock);
        return 0;
    case SO_RCVLOWAT:
        if (unix_read_sockopt_int(optval, optlen, &int_value) < 0) return -EINVAL;
        if (int_value <= 0) return -EINVAL;
        spin_lock(&sk->lock);
        sk->recv_lowat = int_value;
        spin_unlock(&sk->lock);
        return 0;
    case SO_PRIORITY:
        if (unix_read_sockopt_int(optval, optlen, &int_value) < 0) return -EINVAL;
        spin_lock(&sk->lock);
        sk->priority = int_value;
        spin_unlock(&sk->lock);
        return 0;
    case SO_PEEK_OFF:
        if (unix_read_sockopt_int(optval, optlen, &int_value) < 0) return -EINVAL;
        spin_lock(&sk->lock);
        sk->peek_off = int_value;
        spin_unlock(&sk->lock);
        return 0;
    case SO_TIMESTAMP_OLD:
    case SO_TIMESTAMP_NEW:
        if (unix_read_sockopt_int(optval, optlen, &int_value) < 0) return -EINVAL;
        spin_lock(&sk->lock);
        sk->timestamp_legacy = int_value != 0;
        spin_unlock(&sk->lock);
        return 0;
    case SO_TIMESTAMPNS_OLD:
    case SO_TIMESTAMPNS_NEW:
        if (unix_read_sockopt_int(optval, optlen, &int_value) < 0) return -EINVAL;
        spin_lock(&sk->lock);
        sk->timestampns_legacy = int_value != 0;
        spin_unlock(&sk->lock);
        return 0;
    case SO_TIMESTAMPING_OLD:
    case SO_TIMESTAMPING_NEW:
        if (unix_read_sockopt_int(optval, optlen, &int_value) < 0) return -EINVAL;
        spin_lock(&sk->lock);
        sk->timestamping_legacy = int_value != 0;
        spin_unlock(&sk->lock);
        return 0;
    case SO_SNDTIMEO_OLD:
    case SO_SNDTIMEO_NEW:
        if (optval == NULL || optlen < (socklen_t)sizeof(struct timeval)) return -EINVAL;
        spin_lock(&sk->lock);
        memcpy(&sk->send_timeout, optval, sizeof(sk->send_timeout));
        spin_unlock(&sk->lock);
        return 0;
    case SO_RCVTIMEO_OLD:
    case SO_RCVTIMEO_NEW:
        if (optval == NULL || optlen < (socklen_t)sizeof(struct timeval)) return -EINVAL;
        spin_lock(&sk->lock);
        memcpy(&sk->recv_timeout, optval, sizeof(sk->recv_timeout));
        spin_unlock(&sk->lock);
        return 0;
    case SO_LINGER:
        if (optval == NULL || optlen < (socklen_t)sizeof(struct linger)) return -EINVAL;
        spin_lock(&sk->lock);
        memcpy(&sk->linger_opt, optval, sizeof(sk->linger_opt));
        spin_unlock(&sk->lock);
        return 0;
    case SO_DEBUG:
    case SO_DONTROUTE:
    case SO_OOBINLINE:
    case SO_MARK:
        if (unix_read_sockopt_int(optval, optlen, &int_value) < 0) return -EINVAL;
        return 0;
    case SO_PEERCRED:
    case SO_PEERSEC:
    case SO_PEERGROUPS:
    case SO_PEERPIDFD:
        return -ENOPROTOOPT;
    default:
        return -ENOPROTOOPT;
    }
}

int unix_shutdown(void *impl, int how)
{
    struct unix_sock *sk = (struct unix_sock *)impl;
    if (sk == NULL) return -EINVAL;
    (void)how;
    spin_lock(&sk->lock);
    sk->closed = true;
    spin_unlock(&sk->lock);
    return 0;
}

ssize_t unix_read(void *impl, void *buf, size_t size, uint64_t flags)
{
    struct unix_sock *sk = (struct unix_sock *)impl;
    if (sk == NULL || (buf == NULL && size > 0)) return -EINVAL;
    bool nonblock = ((sk->flags & O_NONBLOCK) != 0) || ((flags & MSG_DONTWAIT) != 0);

    if (sk->type == SOCK_DGRAM) {
        spin_lock(&sk->lock);
        if (sk->dg_queue == NULL) sk->dg_queue = queue_init();
        while (sk->dg_queue->head == NULL) {
            if (sk->closed) { spin_unlock(&sk->lock); return 0; }
            if (nonblock) { spin_unlock(&sk->lock); return -EAGAIN; }
            spin_unlock(&sk->lock);
            scheduler_yield();
            spin_lock(&sk->lock);
        }
        struct unix_dgram *dg = (struct unix_dgram *)sk->dg_queue->head->data;
        size_t n = size < dg->len ? size : dg->len;
        if (n > 0) memcpy(buf, dg->data, n);
        queue_remove_node(sk->dg_queue, sk->dg_queue->head);
        unix_dgram_free(dg);
        spin_unlock(&sk->lock);
        return (ssize_t)n;
    }

    spin_lock(&sk->lock);
    if (sk->stream_queue == NULL) sk->stream_queue = queue_init();
    while (sk->stream_queue == NULL || sk->stream_queue->head == NULL) {
        if (sk->closed || sk->peer == NULL) {
            spin_unlock(&sk->lock);
            return 0;
        }
        if (nonblock) {
            spin_unlock(&sk->lock);
            return -EAGAIN;
        }
        spin_unlock(&sk->lock);
        scheduler_yield();
        spin_lock(&sk->lock);
    }

    size_t total = 0;
    while (total < size && sk->stream_queue != NULL && sk->stream_queue->head != NULL) {
        struct unix_stream_packet *packet = (struct unix_stream_packet *)sk->stream_queue->head->data;
        size_t remain = packet->len - packet->offset;
        size_t n = MIN(size - total, remain);
        if (n > 0) {
            memcpy((uint8_t *)buf + total, packet->data + packet->offset, n);
            packet->offset += n;
            total += n;
        }
        if (packet->offset >= packet->len) {
            queue_remove_node(sk->stream_queue, sk->stream_queue->head);
            unix_stream_packet_free(packet);
        }
        if (n == 0) break;
    }
    spin_unlock(&sk->lock);
    return (ssize_t)total;
}

ssize_t unix_write(void *impl, const void *buf, size_t size, uint64_t flags)
{
    (void)flags;
    struct unix_sock *sk = (struct unix_sock *)impl;
    if (sk == NULL || (buf == NULL && size > 0)) return -EINVAL;

    if (sk->type == SOCK_DGRAM) {
        if (sk->peer != NULL)
            return unix_dgram_send_to_peer(sk, sk->peer, (const uint8_t *)buf, size, NULL, 0, NULL, false);

        if (sk->peer_addr.sun_family != AF_UNIX || sk->peer_addr.sun_path[0] == '\0')
            return -EDESTADDRREQ;
        return unix_dgram_send_to_path(sk, sk->peer_addr.sun_path, (const uint8_t *)buf, size, NULL, 0, NULL, false);
    }

    struct unix_sock *peer = sk->peer;
    if (peer == NULL) return -EPIPE;

    spin_lock(&peer->lock);
    if (peer->closed) {
        spin_unlock(&peer->lock);
        return -EPIPE;
    }
    ssize_t ret = unix_stream_enqueue_locked(peer, (const uint8_t *)buf, size, NULL, 0, NULL, false);
    spin_unlock(&peer->lock);
    return ret;
}

ssize_t unix_sendto(void *impl, const void *buf, size_t size, uint64_t flags,
                    const struct sockaddr *addr, socklen_t addrlen)
{
    (void)flags;
    struct unix_sock *sk = (struct unix_sock *)impl;
    if (sk == NULL || (buf == NULL && size > 0)) return -EINVAL;

    if (addr != NULL && addrlen >= (socklen_t)sizeof(sa_family_t)) {
        const struct sockaddr_un *sunaddr = (const struct sockaddr_un *)addr;
        if (sunaddr->sun_family != AF_UNIX) return -EAFNOSUPPORT;
        if (sunaddr->sun_path[0] == '\0') return -EDESTADDRREQ;

        char *peer_path = unix_bound_path_dup(sunaddr, addrlen);
        if (peer_path == NULL) return -EINVAL;
        ssize_t ret = unix_dgram_send_to_path(sk, peer_path, (const uint8_t *)buf, size, NULL, 0, NULL, false);
        free(peer_path);
        return ret;
    }

    if (sk->peer != NULL) return unix_write(impl, buf, size, flags);
    if (sk->peer_addr.sun_family != AF_UNIX) return -EDESTADDRREQ;
    return unix_write(impl, buf, size, flags);
}

ssize_t unix_recvfrom(void *impl, void *buf, size_t size, uint64_t flags,
                      struct sockaddr *addr, socklen_t *addrlen)
{
    struct unix_sock *sk = (struct unix_sock *)impl;
    if (sk == NULL || (buf == NULL && size > 0)) return -EINVAL;

    if (sk->type != SOCK_DGRAM)
    {
        ssize_t ret = unix_read(impl, buf, size, flags);
        if (ret >= 0 && addr != NULL && addrlen != NULL)
        {
            spin_lock(&sk->lock);
            if (sk->peer != NULL)
            {
                socklen_t copy_len =
                    *addrlen < (socklen_t)sizeof(sk->peer_addr) ? *addrlen : (socklen_t)sizeof(sk->peer_addr);
                memcpy(addr, &sk->peer_addr, copy_len);
                *addrlen = sizeof(sk->peer_addr);
            }
            else
            {
                *addrlen = 0;
            }
            spin_unlock(&sk->lock);
        }
        return ret;
    }

    bool nonblock = ((sk->flags & O_NONBLOCK) != 0) || ((flags & MSG_DONTWAIT) != 0);

    spin_lock(&sk->lock);
    if (sk->dg_queue == NULL) sk->dg_queue = queue_init();
    while (sk->dg_queue->head == NULL) {
        if (sk->closed) { spin_unlock(&sk->lock); return 0; }
        if (nonblock) { spin_unlock(&sk->lock); return -EAGAIN; }
        spin_unlock(&sk->lock);
        scheduler_yield();
        spin_lock(&sk->lock);
    }
    struct unix_dgram *dg = (struct unix_dgram *)sk->dg_queue->head->data;
    size_t n = size < dg->len ? size : dg->len;
    if (n > 0) memcpy(buf, dg->data, n);

    if (addr != NULL && addrlen != NULL) {
        socklen_t clen = *addrlen < (socklen_t)sizeof(dg->addr) ? *addrlen : (socklen_t)sizeof(dg->addr);
        memcpy(addr, &dg->addr, clen);
        *addrlen = sizeof(dg->addr);
    }

    queue_remove_node(sk->dg_queue, sk->dg_queue->head);
    unix_dgram_free(dg);
    spin_unlock(&sk->lock);
    return (ssize_t)n;
}

int unix_poll(void *impl, size_t events)
{
    struct unix_sock *sk = (struct unix_sock *)impl;
    if (sk == NULL) return -EINVAL;

    int requested = (int)events;
    int revents = 0;
    spin_lock(&sk->lock);

    if (sk->closed)
        revents |= EPOLLHUP | EPOLLERR;

    if (sk->state == UNIX_LISTENING) {
        if (sk->conn_queue != NULL && sk->conn_queue->head != NULL)
            revents |= EPOLLIN | EPOLLRDNORM;
        if (!sk->closed)
            revents |= EPOLLOUT | EPOLLWRNORM;
        spin_unlock(&sk->lock);
        return revents & (requested | EPOLLERR | EPOLLHUP | EPOLLRDHUP);
    }

    bool has_input = unix_sock_has_input_locked(sk);
    if (has_input)
        revents |= EPOLLIN | EPOLLRDNORM;

    if (sk->type == SOCK_DGRAM) {
        if (!sk->closed)
            revents |= EPOLLOUT | EPOLLWRNORM;

        if (sk->peer != NULL) {
            spin_lock(&sk->peer->lock);
            if (sk->peer->closed) {
                revents |= EPOLLHUP;
                if (requested & EPOLLRDHUP)
                    revents |= EPOLLRDHUP;
            }
            spin_unlock(&sk->peer->lock);
        }

        spin_unlock(&sk->lock);
        return revents & (requested | EPOLLERR | EPOLLHUP | EPOLLRDHUP);
    }

    if (sk->peer != NULL) {
        spin_lock(&sk->peer->lock);
        if (sk->peer->closed) {
            revents |= EPOLLIN | EPOLLRDNORM | EPOLLHUP;
            if (requested & EPOLLRDHUP)
                revents |= EPOLLRDHUP;
        } else if (!sk->closed) {
            revents |= EPOLLOUT | EPOLLWRNORM;
        }
        spin_unlock(&sk->peer->lock);
    } else if (sk->state == UNIX_CONNECTED || sk->closed) {
        revents |= EPOLLIN | EPOLLRDNORM | EPOLLHUP;
        if (requested & EPOLLRDHUP)
            revents |= EPOLLRDHUP;
    }

    spin_unlock(&sk->lock);
    return revents & (requested | EPOLLERR | EPOLLHUP | EPOLLRDHUP);
}

int unix_close(void *impl)
{
    struct unix_sock *sk = (struct unix_sock *)impl;
    if (sk == NULL) return 0;

    char bound_path[UNIX_PATH_MAX];
    bool remove_path_node = false;

    spin_lock(&sk->lock);
    sk->closed = true;

    if (sk->peer != NULL) {
        spin_lock(&sk->peer->lock);
        sk->peer->peer   = NULL;
        spin_unlock(&sk->peer->lock);
        sk->peer = NULL;
    }

    if (sk->state >= UNIX_BOUND)
        unix_ns_unregister(sk);

    if (sk->path_node_created && sk->local_addr.sun_path[0] != '\0') {
        memcpy(bound_path, sk->local_addr.sun_path, sizeof(bound_path));
        bound_path[UNIX_PATH_MAX - 1] = '\0';
        remove_path_node = true;
        sk->path_node_created = false;
    }

    spin_unlock(&sk->lock);

    poll_notify_event();

    if (remove_path_node)
        unix_delete_path_node(bound_path);

    sk->refcount--;
    if (sk->refcount == 0)
        unix_sock_free(sk);
    return 0;
}

int unix_set_flags(void *impl, uint64_t flags)
{
    struct unix_sock *sk = (struct unix_sock *)impl;
    if (sk == NULL) return -EINVAL;
    spin_lock(&sk->lock);
    sk->flags = flags;
    spin_unlock(&sk->lock);
    return 0;
}

ssize_t unix_sendmsg_kernel(void *impl, const uint8_t *buf, size_t size, uint64_t flags,
                            const struct sockaddr *addr, socklen_t addrlen,
                            fd_file_handle **fds, size_t fd_count,
                            const struct ucred *cred, bool has_cred)
{
    struct unix_sock *sk = (struct unix_sock *)impl;
    if (sk == NULL) {
        unix_release_fd_array(fds, fd_count);
        return -EINVAL;
    }
    if (sk->type == SOCK_DGRAM) {
        if (addr != NULL && addrlen >= (socklen_t)sizeof(sa_family_t)) {
            const struct sockaddr_un *sunaddr = (const struct sockaddr_un *)addr;
            if (sunaddr->sun_family != AF_UNIX) {
                unix_release_fd_array(fds, fd_count);
                return -EAFNOSUPPORT;
            }
            if (sunaddr->sun_path[0] == '\0') {
                unix_release_fd_array(fds, fd_count);
                return -EDESTADDRREQ;
            }

            char *peer_path = unix_bound_path_dup(sunaddr, addrlen);
            if (peer_path == NULL) {
                unix_release_fd_array(fds, fd_count);
                return -EINVAL;
            }
            ssize_t ret = unix_dgram_send_to_path(sk, peer_path, buf, size, fds, fd_count, cred, has_cred);
            free(peer_path);
            return ret;
        }

        if (sk->peer != NULL)
            return unix_dgram_send_to_peer(sk, sk->peer, buf, size, fds, fd_count, cred, has_cred);
        if (sk->peer_addr.sun_family == AF_UNIX && sk->peer_addr.sun_path[0] != '\0')
            return unix_dgram_send_to_path(sk, sk->peer_addr.sun_path, buf, size, fds, fd_count, cred, has_cred);

        unix_release_fd_array(fds, fd_count);
        return -EDESTADDRREQ;
    }
    if (addr != NULL) {
        unix_release_fd_array(fds, fd_count);
        return -EISCONN;
    }

    struct unix_sock *peer = sk->peer;
    if (peer == NULL) {
        unix_release_fd_array(fds, fd_count);
        return -EPIPE;
    }

    spin_lock(&peer->lock);
    if (peer->closed) {
        spin_unlock(&peer->lock);
        unix_release_fd_array(fds, fd_count);
        return -EPIPE;
    }
    ssize_t ret = unix_stream_enqueue_locked(peer, buf, size, fds, fd_count, cred, has_cred);
    spin_unlock(&peer->lock);
    if (ret < 0) {
        unix_release_fd_array(fds, fd_count);
    }
    return ret;
}

ssize_t unix_recvmsg_kernel(void *impl, uint8_t *buf, size_t size, uint64_t flags,
                            struct sockaddr *addr, socklen_t *addrlen,
                            fd_file_handle ***out_fds, size_t *out_fd_count,
                            struct ucred *out_cred, bool *out_has_cred)
{
    struct unix_sock *sk = (struct unix_sock *)impl;
    if (sk == NULL || (buf == NULL && size > 0)) return -EINVAL;
    bool nonblock = ((sk->flags & O_NONBLOCK) != 0) || ((flags & MSG_DONTWAIT) != 0);
    bool peek = (flags & MSG_PEEK) != 0;
    if (out_fds != NULL) *out_fds = NULL;
    if (out_fd_count != NULL) *out_fd_count = 0;
    if (out_has_cred != NULL) *out_has_cred = false;

    if (sk->type == SOCK_DGRAM) {
        spin_lock(&sk->lock);
        if (sk->dg_queue == NULL) sk->dg_queue = queue_init();
        while (sk->dg_queue->head == NULL) {
            if (sk->closed) { spin_unlock(&sk->lock); return 0; }
            if (nonblock) { spin_unlock(&sk->lock); return -EAGAIN; }
            spin_unlock(&sk->lock);
            scheduler_yield();
            spin_lock(&sk->lock);
        }
        struct unix_dgram *dg = (struct unix_dgram *)sk->dg_queue->head->data;
        size_t n = size < dg->len ? size : dg->len;
        if (n > 0) memcpy(buf, dg->data, n);

        if (addr != NULL && addrlen != NULL) {
            socklen_t clen = *addrlen < (socklen_t)sizeof(dg->addr) ? *addrlen : (socklen_t)sizeof(dg->addr);
            memcpy(addr, &dg->addr, clen);
            *addrlen = sizeof(dg->addr);
        }
        if (out_fds != NULL && out_fd_count != NULL && dg->fd_count > 0) {
            fd_file_handle **fds = peek ? unix_copy_fd_array(dg->fds, dg->fd_count) : dg->fds;
            if (fds == NULL) {
                spin_unlock(&sk->lock);
                return -ENOMEM;
            }
            *out_fds = fds;
            *out_fd_count = dg->fd_count;
            if (!peek) {
                dg->fds = NULL;
                dg->fd_count = 0;
            }
        }
        if (out_cred != NULL && out_has_cred != NULL && dg->has_cred && sk->passcred) {
            *out_cred = dg->cred;
            *out_has_cred = true;
        }

        if (!peek) {
            queue_remove_node(sk->dg_queue, sk->dg_queue->head);
            unix_dgram_free(dg);
        }
        spin_unlock(&sk->lock);
        return (ssize_t)n;
    }

    spin_lock(&sk->lock);
    if (sk->stream_queue == NULL) sk->stream_queue = queue_init();
    while (sk->stream_queue == NULL || sk->stream_queue->head == NULL) {
        if (sk->closed || sk->peer == NULL) {
            spin_unlock(&sk->lock);
            return 0;
        }
        if (nonblock) {
            spin_unlock(&sk->lock);
            return -EAGAIN;
        }
        spin_unlock(&sk->lock);
        scheduler_yield();
        spin_lock(&sk->lock);
    }

    if (addr != NULL && addrlen != NULL && sk->peer != NULL) {
        socklen_t copy_len = *addrlen < (socklen_t)sizeof(sk->peer_addr) ? *addrlen : (socklen_t)sizeof(sk->peer_addr);
        memcpy(addr, &sk->peer_addr, copy_len);
        *addrlen = sizeof(sk->peer_addr);
    }

    size_t total = 0;
    bool ancillary_taken = false;
    lock_node *stream_node = sk->stream_queue != NULL ? sk->stream_queue->head : NULL;
    while (total < size && stream_node != NULL) {
        struct unix_stream_packet *packet = (struct unix_stream_packet *)stream_node->data;
        lock_node *next_node = stream_node->next;
        if (packet == NULL) {
            if (!peek) queue_remove_node(sk->stream_queue, stream_node);
            stream_node = next_node;
            continue;
        }
        if (!ancillary_taken && packet->fd_count > 0 && out_fds != NULL && out_fd_count != NULL) {
            fd_file_handle **fds = peek ? unix_copy_fd_array(packet->fds, packet->fd_count) : packet->fds;
            if (fds == NULL) {
                spin_unlock(&sk->lock);
                return -ENOMEM;
            }
            *out_fds = fds;
            *out_fd_count = packet->fd_count;
            if (!peek) {
                packet->fds = NULL;
                packet->fd_count = 0;
            }
            ancillary_taken = true;
        }
        if (out_cred != NULL && out_has_cred != NULL && !*out_has_cred && packet->has_cred && sk->passcred) {
            *out_cred = packet->cred;
            *out_has_cred = true;
        }

        size_t read_offset = packet->offset;
        size_t remain = packet->len - read_offset;
        size_t n = MIN(size - total, remain);
        if (n > 0) {
            memcpy(buf + total, packet->data + read_offset, n);
            if (!peek) packet->offset += n;
            total += n;
        }
        if (!peek && packet->offset >= packet->len) {
            queue_remove_node(sk->stream_queue, stream_node);
            unix_stream_packet_free(packet);
        }
        stream_node = next_node;
        if (n == 0) break;
    }

    spin_unlock(&sk->lock);
    return (ssize_t)total;
}

int unix_socketpair(int type, int protocol, int sv[2])
{
    if (sv == NULL) return -EINVAL;
    if (type != SOCK_STREAM && type != SOCK_DGRAM && type != SOCK_SEQPACKET) return -EPROTONOSUPPORT;

    init_unix_provider();

    void *s1 = NULL, *s2 = NULL;
    int ret = unix_create(AF_UNIX, type, protocol, &s1);
    if (ret < 0) return ret;
    ret = unix_create(AF_UNIX, type, protocol, &s2);
    if (ret < 0) { unix_close(s1); return ret; }

    struct unix_sock *sk1 = (struct unix_sock *)s1;
    struct unix_sock *sk2 = (struct unix_sock *)s2;

    sk1->state = UNIX_CONNECTED;
    sk2->state = UNIX_CONNECTED;
    sk1->owner_pid = get_current_task() != NULL && get_current_task()->parent_group != NULL
                         ? (int)get_current_task()->parent_group->pid
                         : 0;
    sk1->owner_uid = user_uid(task_effective_user());
    sk1->owner_gid = user_gid(task_effective_user());
    sk2->owner_pid = sk1->owner_pid;
    sk2->owner_uid = sk1->owner_uid;
    sk2->owner_gid = sk1->owner_gid;
    sk1->local_addr.sun_family = AF_UNIX;
    sk2->local_addr.sun_family = AF_UNIX;
    sk1->peer = sk2;
    sk2->peer = sk1;
    sk2->refcount++;
    sk1->refcount++;

    sv[0] = socketfs_alloc_fd_with_provider(s1, 0, &g_unix_provider);
    sv[1] = socketfs_alloc_fd_with_provider(s2, 0, &g_unix_provider);
    if (sv[0] < 0 || sv[1] < 0) {
        unix_close(s1);
        unix_close(s2);
        return -EMFILE;
    }
    return 0;
}

void unix_unbind_path(const char *path)
{
    if (path == NULL || path[0] == '\0') return;

    struct unix_sock *bound = unix_ns_lookup(path);
    if (bound != NULL) {
        spin_lock(&bound->lock);
        bound->path_node_created = false;
        unix_ns_unregister(bound);
        spin_unlock(&bound->lock);
        bound->refcount--;
    }

    unix_delete_path_node(path);
}
