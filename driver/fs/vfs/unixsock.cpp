#define ALL_IMPLEMENTATION
#include <net/unixsock.h>
#include <net/socket.h>
#include <errno.h>
#include <krlibc.h>
#include <mm/heap.h>
#include <task/pcb.h>
#include <task/poll.h>

socket_provider_t g_unix_provider;

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
    g_unix_provider.connect      = unix_connect;
    g_unix_provider.listen       = unix_listen;
    g_unix_provider.accept       = unix_accept;
    g_unix_provider.shutdown     = unix_shutdown;
    g_unix_provider.poll         = unix_poll;
    g_unix_provider.set_flags    = unix_set_flags;
}

static lock_queue *g_unix_namespace = NULL;
static spin_t      g_unix_ns_lock   = SPIN_INIT;

static void unix_sock_init(struct unix_sock *sk, int type, int protocol)
{
    sk->state    = UNIX_FREE;
    sk->type     = type;
    sk->protocol = protocol;
    sk->peer     = NULL;
    sk->conn_queue = NULL;
    sk->rcap = UNIX_RING_SIZE;
    sk->wcap = UNIX_RING_SIZE;
    sk->rbuf = (uint8_t *)malloc(UNIX_RING_SIZE);
    sk->wbuf = (uint8_t *)malloc(UNIX_RING_SIZE);
    sk->rhead = sk->rtail = 0;
    sk->whead = sk->wtail = 0;
    sk->dg_queue = NULL;
    sk->lock     = SPIN_INIT;
    sk->refcount = 1;
    sk->closed   = false;
    memset(&sk->local_addr, 0, sizeof(sk->local_addr));
    memset(&sk->peer_addr, 0, sizeof(sk->peer_addr));
}

static void unix_sock_free(struct unix_sock *sk)
{
    if (sk == NULL) return;
    if (sk->rbuf) { free(sk->rbuf); sk->rbuf = NULL; }
    if (sk->wbuf) { free(sk->wbuf); sk->wbuf = NULL; }
    if (sk->conn_queue) {
        while (sk->conn_queue->head) {
            struct unix_sock *cs = (struct unix_sock *)sk->conn_queue->head->data;
            if (cs) {
                spin_lock(&cs->lock);
                cs->peer = NULL;
                cs->state = UNIX_FREE;
                spin_unlock(&cs->lock);
            }
            queue_remove_node(sk->conn_queue, sk->conn_queue->head);
        }
        free(sk->conn_queue);
    }
    if (sk->dg_queue) {
        while (sk->dg_queue->head) {
            struct unix_dgram *dg = (struct unix_dgram *)sk->dg_queue->head->data;
            queue_remove_node(sk->dg_queue, sk->dg_queue->head);
            free(dg);
        }
        free(sk->dg_queue);
    }
    free(sk);
}

static void unix_ns_register(struct unix_sock *sk)
{
    if (sk->local_addr.sun_path[0] == '\0') return;
    spin_lock(&g_unix_ns_lock);
    if (g_unix_namespace == NULL)
        g_unix_namespace = queue_init();
    if (g_unix_namespace != NULL)
        queue_enqueue(g_unix_namespace, sk);
    spin_unlock(&g_unix_ns_lock);
}

static void unix_ns_unregister(struct unix_sock *sk)
{
    if (g_unix_namespace == NULL) return;
    spin_lock(&g_unix_ns_lock);
    lock_node *node = g_unix_namespace->head;
    while (node != NULL) {
        if (node->data == sk) {
            queue_remove_node(g_unix_namespace, node);
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
    if (sk->whead >= sk->wtail) return sk->whead - sk->wtail;
    return sk->wcap - sk->wtail + sk->whead;
}

static size_t ring_writable(struct unix_sock *sk)
{
    size_t used = (sk->rhead >= sk->rtail) ? (sk->rhead - sk->rtail)
                                           : (sk->rcap - sk->rtail + sk->rhead);
    size_t free_space = sk->rcap - used;
    return free_space > 1 ? free_space - 1 : 0;
}

static size_t ring_write(struct unix_sock *sk, const uint8_t *data, size_t len)
{
    size_t avail = ring_writable(sk);
    if (avail == 0 || len == 0) return 0;
    size_t n = len < avail ? len : avail;
    for (size_t i = 0; i < n; i++) {
        sk->rbuf[sk->rhead] = data[i];
        sk->rhead = (sk->rhead + 1) % sk->rcap;
    }
    return n;
}

static size_t ring_read(struct unix_sock *sk, uint8_t *data, size_t len)
{
    size_t avail = ring_readable(sk);
    if (avail == 0 || len == 0) return 0;
    size_t n = len < avail ? len : avail;
    for (size_t i = 0; i < n; i++) {
        data[i] = sk->wbuf[sk->wtail];
        sk->wtail = (sk->wtail + 1) % sk->wcap;
    }
    return n;
}

int unix_create(int domain, int type, int protocol, void **out_impl)
{
    (void)domain;
    if (out_impl == NULL) return -EINVAL;
    if (type != SOCK_STREAM && type != SOCK_DGRAM) return -EPROTONOSUPPORT;

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

    if (max_path > 0 && sunaddr->sun_path[0] != '\0') {
        struct unix_sock *existing = unix_ns_lookup(sunaddr->sun_path);
        if (existing != NULL) {
            existing->refcount--;
            spin_unlock(&sk->lock);
            return -EADDRINUSE;
        }
    }

    memcpy(&sk->local_addr, sunaddr, addrlen < sizeof(sk->local_addr) ? (size_t)addrlen : sizeof(sk->local_addr));
    sk->state = UNIX_BOUND;

    if (max_path > 0 && sunaddr->sun_path[0] != '\0')
        unix_ns_register(sk);

    spin_unlock(&sk->lock);
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

    memcpy(&sk->peer_addr, sunaddr, addrlen < sizeof(sk->peer_addr) ? (size_t)addrlen : sizeof(sk->peer_addr));

    if (sk->type == SOCK_DGRAM) {
        sk->state = UNIX_CONNECTED;
        spin_unlock(&sk->lock);
        return 0;
    }

    struct unix_sock *listener = NULL;
    if (sunaddr->sun_path[0] != '\0')
        listener = unix_ns_lookup(sunaddr->sun_path);

    if (listener == NULL) { spin_unlock(&sk->lock); return -ECONNREFUSED; }

    spin_lock(&listener->lock);
    if (listener->state != UNIX_LISTENING || listener->conn_queue == NULL) {
        spin_unlock(&listener->lock);
        listener->refcount--;
        spin_unlock(&sk->lock);
        return -ECONNREFUSED;
    }

    sk->state = UNIX_CONNECTED;
    sk->peer  = listener;
    queue_enqueue(listener->conn_queue, sk);
    spin_unlock(&listener->lock);
    spin_unlock(&sk->lock);
    return 0;
}

int unix_listen(void *impl, int backlog)
{
    struct unix_sock *sk = (struct unix_sock *)impl;
    if (sk == NULL) return -EINVAL;

    spin_lock(&sk->lock);
    if (sk->type != SOCK_STREAM) { spin_unlock(&sk->lock); return -EOPNOTSUPP; }
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
        spin_unlock(&sk->lock);
        scheduler_yield();
        spin_lock(&sk->lock);
        if (sk->state != UNIX_LISTENING) { spin_unlock(&sk->lock); return -EINVAL; }
    }

    struct unix_sock *client = (struct unix_sock *)sk->conn_queue->head->data;
    queue_remove_node(sk->conn_queue, sk->conn_queue->head);

    spin_lock(&client->lock);
    struct unix_sock *new_sk = (struct unix_sock *)malloc(sizeof(struct unix_sock));
    if (new_sk == NULL) {
        spin_unlock(&client->lock);
        spin_unlock(&sk->lock);
        return -ENOMEM;
    }
    unix_sock_init(new_sk, SOCK_STREAM, 0);
    new_sk->state = UNIX_CONNECTED;
    new_sk->local_addr = sk->local_addr;
    new_sk->peer_addr  = client->local_addr;

    new_sk->rbuf = client->wbuf; client->wbuf = NULL;
    new_sk->wbuf = client->rbuf; client->rbuf = NULL;
    new_sk->rcap = new_sk->wcap = UNIX_RING_SIZE;
    new_sk->rhead = client->whead; new_sk->rtail = client->wtail;
    new_sk->whead = client->rhead; new_sk->wtail = client->rtail;

    client->peer  = new_sk;
    new_sk->peer  = client;
    client->state = UNIX_CONNECTED;
    new_sk->local_addr.sun_family = AF_UNIX;
    spin_unlock(&client->lock);

    if (addr != NULL && addrlen != NULL) {
        socklen_t copy_len = sizeof(client->local_addr);
        if (*addrlen < copy_len) copy_len = *addrlen;
        memcpy(addr, &client->local_addr, copy_len);
        *addrlen = sizeof(client->local_addr);
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

int unix_getsockopt(void *impl, int level, int optname, void *optval, socklen_t *optlen)
{
    struct unix_sock *sk = (struct unix_sock *)impl;
    if (sk == NULL || optval == NULL || optlen == NULL) return -EINVAL;

    if (level == SOL_SOCKET) {
        switch (optname) {
        case SO_TYPE: {
            int val = sk->type;
            size_t copy_len = *optlen < (socklen_t)sizeof(val) ? (size_t)*optlen : sizeof(val);
            memcpy(optval, &val, copy_len);
            *optlen = (socklen_t)sizeof(val);
            return 0;
        }
        case SO_ERROR: {
            int val = 0;
            size_t copy_len = *optlen < (socklen_t)sizeof(val) ? (size_t)*optlen : sizeof(val);
            memcpy(optval, &val, copy_len);
            *optlen = (socklen_t)sizeof(val);
            return 0;
        }
        default: return -ENOPROTOOPT;
        }
    }
    return -ENOPROTOOPT;
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
    (void)flags;
    struct unix_sock *sk = (struct unix_sock *)impl;
    if (sk == NULL || buf == NULL) return -EINVAL;

    if (sk->type == SOCK_DGRAM) {
        spin_lock(&sk->lock);
        if (sk->dg_queue == NULL) sk->dg_queue = queue_init();
        while (sk->dg_queue->head == NULL) {
            if (sk->closed) { spin_unlock(&sk->lock); return 0; }
            spin_unlock(&sk->lock);
            scheduler_yield();
            spin_lock(&sk->lock);
        }
        struct unix_dgram *dg = (struct unix_dgram *)sk->dg_queue->head->data;
        size_t n = size < dg->len ? size : dg->len;
        memcpy(buf, dg->data, n);
        queue_remove_node(sk->dg_queue, sk->dg_queue->head);
        free(dg);
        spin_unlock(&sk->lock);
        return (ssize_t)n;
    }

    spin_lock(&sk->lock);
    while (ring_readable(sk) == 0) {
        if (sk->closed || sk->peer == NULL) {
            spin_unlock(&sk->lock);
            return 0;
        }
        spin_unlock(&sk->lock);
        scheduler_yield();
        spin_lock(&sk->lock);
    }
    size_t n = ring_read(sk, (uint8_t *)buf, size);
    spin_unlock(&sk->lock);
    return (ssize_t)n;
}

ssize_t unix_write(void *impl, const void *buf, size_t size, uint64_t flags)
{
    (void)flags;
    struct unix_sock *sk = (struct unix_sock *)impl;
    if (sk == NULL || buf == NULL) return -EINVAL;

    if (sk->type == SOCK_DGRAM) {
        if (sk->peer_addr.sun_family != AF_UNIX || sk->peer_addr.sun_path[0] == '\0')
            return -EDESTADDRREQ;
        struct unix_sock *peer = unix_ns_lookup(sk->peer_addr.sun_path);
        if (peer == NULL) return -ECONNREFUSED;
        if (peer->dg_queue == NULL) {
            spin_lock(&peer->lock);
            if (peer->dg_queue == NULL) peer->dg_queue = queue_init();
            spin_unlock(&peer->lock);
        }
        struct unix_dgram *dg = (struct unix_dgram *)malloc(sizeof(struct unix_dgram) + size);
        if (dg == NULL) { peer->refcount--; return -ENOMEM; }
        memcpy(&dg->addr, &sk->local_addr, sizeof(dg->addr));
        dg->len = size;
        memcpy(dg->data, buf, size);
        spin_lock(&peer->lock);
        queue_enqueue(peer->dg_queue, dg);
        spin_unlock(&peer->lock);
        peer->refcount--;
        return (ssize_t)size;
    }

    struct unix_sock *peer = sk->peer;
    if (peer == NULL) return -EPIPE;

    const uint8_t *src = (const uint8_t *)buf;
    spin_lock(&peer->lock);
    size_t total = 0;
    while (total < size) {
        size_t n = ring_write(peer, src + total, size - total);
        if (n == 0) {
            if (peer->closed) { spin_unlock(&peer->lock); return -EPIPE; }
            spin_unlock(&peer->lock);
            scheduler_yield();
            spin_lock(&peer->lock);
            continue;
        }
        total += n;
    }
    spin_unlock(&peer->lock);
    return (ssize_t)total;
}

ssize_t unix_sendto(void *impl, const void *buf, size_t size, uint64_t flags,
                    const struct sockaddr *addr, socklen_t addrlen)
{
    (void)flags;
    struct unix_sock *sk = (struct unix_sock *)impl;
    if (sk == NULL || buf == NULL) return -EINVAL;

    if (addr != NULL && addrlen >= (socklen_t)sizeof(sa_family_t)) {
        const struct sockaddr_un *sunaddr = (const struct sockaddr_un *)addr;
        if (sunaddr->sun_family != AF_UNIX) return -EAFNOSUPPORT;
        if (sunaddr->sun_path[0] == '\0') return -EDESTADDRREQ;

        struct unix_sock *peer = unix_ns_lookup(sunaddr->sun_path);
        if (peer == NULL) return -ECONNREFUSED;

        if (peer->dg_queue == NULL) {
            spin_lock(&peer->lock);
            if (peer->dg_queue == NULL) peer->dg_queue = queue_init();
            spin_unlock(&peer->lock);
        }
        struct unix_dgram *dg = (struct unix_dgram *)malloc(sizeof(struct unix_dgram) + size);
        if (dg == NULL) { peer->refcount--; return -ENOMEM; }
        if (sk->local_addr.sun_family == AF_UNIX)
            memcpy(&dg->addr, &sk->local_addr, sizeof(dg->addr));
        else
            memset(&dg->addr, 0, sizeof(dg->addr));
        dg->addr.sun_family = AF_UNIX;
        dg->len = size;
        memcpy(dg->data, buf, size);
        spin_lock(&peer->lock);
        queue_enqueue(peer->dg_queue, dg);
        spin_unlock(&peer->lock);
        peer->refcount--;
        return (ssize_t)size;
    }

    if (sk->peer_addr.sun_family != AF_UNIX) return -EDESTADDRREQ;
    return unix_write(impl, buf, size, flags);
}

ssize_t unix_recvfrom(void *impl, void *buf, size_t size, uint64_t flags,
                      struct sockaddr *addr, socklen_t *addrlen)
{
    (void)flags;
    struct unix_sock *sk = (struct unix_sock *)impl;
    if (sk == NULL || buf == NULL) return -EINVAL;

    spin_lock(&sk->lock);
    if (sk->dg_queue == NULL) sk->dg_queue = queue_init();
    while (sk->dg_queue->head == NULL) {
        if (sk->closed) { spin_unlock(&sk->lock); return 0; }
        spin_unlock(&sk->lock);
        scheduler_yield();
        spin_lock(&sk->lock);
    }
    struct unix_dgram *dg = (struct unix_dgram *)sk->dg_queue->head->data;
    size_t n = size < dg->len ? size : dg->len;
    memcpy(buf, dg->data, n);

    if (addr != NULL && addrlen != NULL) {
        socklen_t clen = *addrlen < (socklen_t)sizeof(dg->addr) ? *addrlen : (socklen_t)sizeof(dg->addr);
        memcpy(addr, &dg->addr, clen);
        *addrlen = sizeof(dg->addr);
    }

    queue_remove_node(sk->dg_queue, sk->dg_queue->head);
    free(dg);
    spin_unlock(&sk->lock);
    return (ssize_t)n;
}

int unix_poll(void *impl, size_t events)
{
    struct unix_sock *sk = (struct unix_sock *)impl;
    if (sk == NULL) return -EINVAL;

    int revents = 0;
    spin_lock(&sk->lock);

    if (sk->type == SOCK_DGRAM) {
        if (sk->dg_queue != NULL && sk->dg_queue->head != NULL)
            revents |= POLLIN;
    } else {
        if (ring_readable(sk) > 0)
            revents |= POLLIN;
    }

    if (sk->peer != NULL) {
        spin_lock(&sk->peer->lock);
        if (ring_writable(sk->peer) > 0)
            revents |= POLLOUT;
        spin_unlock(&sk->peer->lock);
    } else if (sk->type == SOCK_DGRAM) {
        revents |= POLLOUT;
    }

    if (sk->closed)
        revents |= POLLHUP;

    if (sk->state == UNIX_LISTENING && sk->conn_queue != NULL && sk->conn_queue->head != NULL)
        revents |= POLLIN;

    spin_unlock(&sk->lock);
    return revents & (int)events;
}

int unix_close(void *impl)
{
    struct unix_sock *sk = (struct unix_sock *)impl;
    if (sk == NULL) return 0;

    spin_lock(&sk->lock);
    sk->closed = true;

    if (sk->peer != NULL) {
        spin_lock(&sk->peer->lock);
        sk->peer->closed = true;
        sk->peer->peer   = NULL;
        spin_unlock(&sk->peer->lock);
        sk->peer = NULL;
    }

    if (sk->state >= UNIX_BOUND)
        unix_ns_unregister(sk);

    spin_unlock(&sk->lock);

    sk->refcount--;
    if (sk->refcount == 0)
        unix_sock_free(sk);
    return 0;
}

int unix_set_flags(void *impl, uint64_t flags)
{
    (void)impl; (void)flags;
    return 0;
}

int unix_socketpair(int type, int protocol, int sv[2])
{
    if (sv == NULL) return -EINVAL;
    if (type != SOCK_STREAM && type != SOCK_DGRAM) return -EPROTONOSUPPORT;

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
    sk1->local_addr.sun_family = AF_UNIX;
    sk2->local_addr.sun_family = AF_UNIX;
    sk1->peer = sk2;
    sk2->peer = sk1;
    sk2->refcount++;
    sk1->refcount++;

    if (type == SOCK_STREAM) {
        uint8_t *tmp = sk1->wbuf;
        sk1->wbuf = sk2->rbuf;
        sk2->rbuf = tmp;
        tmp = sk1->rbuf;
        sk1->rbuf = sk2->wbuf;
        sk2->wbuf = tmp;
    }

    sv[0] = socketfs_alloc_fd_with_provider(s1, 0, &g_unix_provider);
    sv[1] = socketfs_alloc_fd_with_provider(s2, 0, &g_unix_provider);
    if (sv[0] < 0 || sv[1] < 0) {
        unix_close(s1);
        unix_close(s2);
        return -EMFILE;
    }
    return 0;
}
