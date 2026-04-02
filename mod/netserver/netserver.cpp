extern "C" {
#include "../../include/dlinker.h"
#include "../../include/errno.h"
#include "../../include/krlibc.h"
#include "../../include/net/socket.h"
#include "../../include/netdev.h"
#include "../../include/task/pcb.h"
#include "../../include/task/poll.h"
#include "../../include/task/scheduler.h"
#include "lwip/api.h"
#include "lwip/def.h"
#include "lwip/dhcp.h"
#include "lwip/etharp.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "lwip/netifapi.h"
#include "lwip/pbuf.h"
#include "lwip/tcpip.h"
#include "netif/ethernet.h"
}

typedef struct xj380_lwip_socket {
    struct netconn *conn;
    struct pbuf    *rx_buf;
    bool            is_listener;
} xj380_lwip_socket_t;

static struct netif g_netif;
static netdev_t    *g_netdev              = NULL;
static bool         g_stack_ready         = false;
static bool         g_worker_started      = false;
static bool         g_provider_registered = false;
static bool         g_dhcp_ready_logged   = false;
static uint32_t     g_rx_log_count        = 0;
static uint32_t     g_tx_log_count        = 0;

static int xj380_err_to_errno(err_t err)
{
    if (err == ERR_OK) {
        return 0;
    }
    return -err_to_errno(err);
}

static err_t xj380_linkoutput(struct netif *netif, struct pbuf *p)
{
    netdev_t *dev = (netdev_t *)netif->state;
    if (dev == NULL || p == NULL) {
        return ERR_IF;
    }

    uint8_t frame[2048];
    if (p->tot_len > sizeof(frame)) {
        return ERR_BUF;
    }

    size_t copied = 0;
    for (struct pbuf *cursor = p; cursor != NULL; cursor = cursor->next) {
        memcpy(frame + copied, cursor->payload, cursor->len);
        copied += cursor->len;
    }

    if (g_tx_log_count < 24 && copied >= 14) {
        uint16_t eth_type = ((uint16_t)frame[12] << 8) | frame[13];
        printk("netserver: tx frame len=%u type=0x%04x dst=%02x:%02x:%02x:%02x:%02x:%02x src=%02x:%02x:%02x:%02x:%02x:%02x\n",
               (unsigned int)copied,
               (unsigned int)eth_type,
               frame[0], frame[1], frame[2], frame[3], frame[4], frame[5],
               frame[6], frame[7], frame[8], frame[9], frame[10], frame[11]);
        g_tx_log_count++;
    }

    int sent = dev->send(dev->desc, frame, (uint32_t)copied);
    return sent == (int)copied ? ERR_OK : ERR_IF;
}

static err_t xj380_netif_init(struct netif *netif)
{
    netif->name[0]    = 'e';
    netif->name[1]    = '0';
    netif->hwaddr_len = ETH_HWADDR_LEN;
    memcpy(netif->hwaddr, g_netdev->mac, ETH_HWADDR_LEN);
    netif->mtu        = 1500;
    netif->flags      = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_ETHERNET;
    netif->output     = etharp_output;
    netif->linkoutput = xj380_linkoutput;
    return ERR_OK;
}

static int xj380_sockaddr_to_ip(const struct sockaddr *addr, socklen_t addrlen, ip_addr_t *ip, u16_t *port)
{
    if (addr == NULL || ip == NULL || port == NULL || addrlen < sizeof(struct sockaddr_in)) {
        return -EINVAL;
    }

    const struct sockaddr_in *sin = (const struct sockaddr_in *)addr;
    if (sin->sin_family != AF_INET) {
        return -EAFNOSUPPORT;
    }

    ip4_addr_t ip4;
    ip4.addr = sin->sin_addr.s_addr;
    ip_addr_copy_from_ip4(*ip, ip4);
    *port = lwip_ntohs(sin->sin_port);
    return 0;
}

static void xj380_ip_to_sockaddr(const ip_addr_t *ip, u16_t port, struct sockaddr *addr, socklen_t *addrlen)
{
    if (addrlen == NULL) {
        return;
    }
    if (*addrlen < sizeof(struct sockaddr_in) || addr == NULL) {
        *addrlen = sizeof(struct sockaddr_in);
        return;
    }

    struct sockaddr_in out;
    memset(&out, 0, sizeof(out));
    out.sin_family = AF_INET;
    out.sin_port   = lwip_htons(port);
    if (ip != NULL && IP_IS_V4(ip)) {
        out.sin_addr.s_addr = ip_2_ip4(ip)->addr;
    }

    memcpy(addr, &out, sizeof(out));
    *addrlen = sizeof(out);
}

static bool xj380_mbox_has_data(sys_mbox_t *mbox)
{
    bool ready;
    spin_lock(&mbox->lock);
    ready = mbox->ptrRead != mbox->ptrWrite;
    spin_unlock(&mbox->lock);
    return ready;
}

static ssize_t xj380_socket_read_impl(xj380_lwip_socket_t *socket, void *buffer, size_t size, uint64_t flags)
{
    u8_t    api_flags = NETCONN_NOAUTORCVD;
    ssize_t recvd     = 0;
    size_t  left      = size;

    if (flags & O_NONBLOCK) {
        api_flags |= NETCONN_DONTBLOCK;
    }

    while (left > 0) {
        struct pbuf *p = socket->rx_buf;
        if (p == NULL) {
            err_t err = netconn_recv_tcp_pbuf_flags(socket->conn, &p, api_flags);
            if (err != ERR_OK) {
                if (recvd > 0) {
                    break;
                }
                if (err == ERR_CLSD) {
                    return 0;
                }
                return xj380_err_to_errno(err);
            }
            socket->rx_buf = p;
        }

        u16_t copy_len = (u16_t)MIN(left, (size_t)p->tot_len);
        pbuf_copy_partial(p, (uint8_t *)buffer + recvd, copy_len, 0);
        recvd += copy_len;
        left  -= copy_len;

        if (p->tot_len > copy_len) {
            socket->rx_buf = pbuf_free_header(p, copy_len);
        } else {
            socket->rx_buf = NULL;
            pbuf_free(p);
        }

        api_flags |= NETCONN_DONTBLOCK | NETCONN_NOFIN;
        if (recvd > 0) {
            break;
        }
    }

    if (recvd > 0) {
        netconn_tcp_recvd(socket->conn, (size_t)recvd);
    }
    return recvd;
}

static int xj380_provider_create(int domain, int type, int protocol, void **out_socket)
{
    if (out_socket == NULL) {
        return -EINVAL;
    }
    if (!g_stack_ready) {
        for (int retry = 0; retry < 500 && !g_stack_ready; ++retry) {
            delay_ms_hp(10);
        }
        if (!g_stack_ready) {
            return -ENETDOWN;
        }
    }
    if (domain != AF_INET || type != SOCK_STREAM) {
        return -EAFNOSUPPORT;
    }
    if (protocol != 0 && protocol != 6) {
        return -EPROTONOSUPPORT;
    }

    struct netconn *conn = netconn_new(NETCONN_TCP);
    if (conn == NULL) {
        return -ENOMEM;
    }

    xj380_lwip_socket_t *socket = (xj380_lwip_socket_t *)calloc(1, sizeof(xj380_lwip_socket_t));
    if (socket == NULL) {
        netconn_delete(conn);
        return -ENOMEM;
    }

    socket->conn = conn;
    *out_socket  = socket;
    return 0;
}

static int xj380_provider_close(void *socket_ptr)
{
    xj380_lwip_socket_t *socket = (xj380_lwip_socket_t *)socket_ptr;
    if (socket == NULL) {
        return -EINVAL;
    }

    if (socket->rx_buf != NULL) {
        pbuf_free(socket->rx_buf);
    }
    if (socket->conn != NULL) {
        netconn_delete(socket->conn);
    }
    free(socket);
    return 0;
}

static ssize_t xj380_provider_read(void *socket_ptr, void *buffer, size_t size, uint64_t flags)
{
    xj380_lwip_socket_t *socket = (xj380_lwip_socket_t *)socket_ptr;
    if (socket == NULL || buffer == NULL) {
        return -EINVAL;
    }
    return xj380_socket_read_impl(socket, buffer, size, flags);
}

static ssize_t xj380_provider_write(void *socket_ptr, const void *buffer, size_t size, uint64_t flags)
{
    xj380_lwip_socket_t *socket = (xj380_lwip_socket_t *)socket_ptr;
    if (socket == NULL || buffer == NULL) {
        return -EINVAL;
    }

    size_t written   = 0;
    u8_t   api_flags = NETCONN_COPY;
    if (flags & O_NONBLOCK) {
        api_flags |= NETCONN_DONTBLOCK;
    }

    err_t err = netconn_write_partly(socket->conn, buffer, size, api_flags, &written);
    if (err != ERR_OK) {
        if (written > 0) {
            return (ssize_t)written;
        }
        return xj380_err_to_errno(err);
    }
    return (ssize_t)written;
}

static int xj380_provider_bind(void *socket_ptr, const struct sockaddr *addr, socklen_t addrlen)
{
    xj380_lwip_socket_t *socket = (xj380_lwip_socket_t *)socket_ptr;
    ip_addr_t            ip;
    u16_t                port;

    if (socket == NULL) {
        return -EINVAL;
    }

    int ret = xj380_sockaddr_to_ip(addr, addrlen, &ip, &port);
    if (ret < 0) {
        return ret;
    }

    return xj380_err_to_errno(netconn_bind(socket->conn, &ip, port));
}

static int xj380_provider_connect(void *socket_ptr, const struct sockaddr *addr, socklen_t addrlen)
{
    xj380_lwip_socket_t *socket = (xj380_lwip_socket_t *)socket_ptr;
    ip_addr_t            ip;
    u16_t                port;

    if (socket == NULL) {
        return -EINVAL;
    }

    int ret = xj380_sockaddr_to_ip(addr, addrlen, &ip, &port);
    if (ret < 0) {
        return ret;
    }

    return xj380_err_to_errno(netconn_connect(socket->conn, &ip, port));
}

static int xj380_provider_listen(void *socket_ptr, int backlog)
{
    xj380_lwip_socket_t *socket = (xj380_lwip_socket_t *)socket_ptr;
    if (socket == NULL) {
        return -EINVAL;
    }

    err_t err = netconn_listen_with_backlog(socket->conn, (u8_t)backlog);
    if (err != ERR_OK) {
        return xj380_err_to_errno(err);
    }
    socket->is_listener = true;
    return 0;
}

static int xj380_provider_accept(void *socket_ptr, void **out_socket, struct sockaddr *addr, socklen_t *addrlen)
{
    xj380_lwip_socket_t *listener = (xj380_lwip_socket_t *)socket_ptr;
    if (listener == NULL || out_socket == NULL) {
        return -EINVAL;
    }

    struct netconn *new_conn = NULL;
    err_t           err      = netconn_accept(listener->conn, &new_conn);
    if (err != ERR_OK) {
        return xj380_err_to_errno(err);
    }

    xj380_lwip_socket_t *socket = (xj380_lwip_socket_t *)calloc(1, sizeof(xj380_lwip_socket_t));
    if (socket == NULL) {
        netconn_delete(new_conn);
        return -ENOMEM;
    }

    socket->conn = new_conn;
    if (netconn_is_nonblocking(listener->conn)) {
        netconn_set_nonblocking(socket->conn, 1);
    }

    if (addrlen != NULL) {
        ip_addr_t peer_addr;
        u16_t     peer_port = 0;
        if (netconn_peer(new_conn, &peer_addr, &peer_port) == ERR_OK) {
            xj380_ip_to_sockaddr(&peer_addr, peer_port, addr, addrlen);
        }
    }

    *out_socket = socket;
    return 0;
}

static int xj380_provider_shutdown(void *socket_ptr, int how)
{
    xj380_lwip_socket_t *socket = (xj380_lwip_socket_t *)socket_ptr;
    if (socket == NULL) {
        return -EINVAL;
    }

    int shut_rx = how != SHUT_WR;
    int shut_tx = how != SHUT_RD;
    return xj380_err_to_errno(netconn_shutdown(socket->conn, shut_rx, shut_tx));
}

static int xj380_provider_poll(void *socket_ptr, size_t events)
{
    xj380_lwip_socket_t *socket = (xj380_lwip_socket_t *)socket_ptr;
    if (socket == NULL) {
        return -EINVAL;
    }

    int out = 0;

    if (events & EPOLLIN) {
        if (socket->rx_buf != NULL) {
            out |= EPOLLIN;
        } else if (socket->is_listener) {
            if (xj380_mbox_has_data(&socket->conn->acceptmbox)) {
                out |= EPOLLIN;
            }
        } else if (xj380_mbox_has_data(&socket->conn->recvmbox) || socket->conn->pending_err == ERR_CLSD) {
            out |= EPOLLIN;
        }
    }

    if (events & EPOLLOUT) {
        if (socket->conn->pending_err == ERR_OK) {
            out |= EPOLLOUT;
        }
    }

    if (socket->conn->pending_err == ERR_CLSD) {
        out |= EPOLLHUP;
    } else if (socket->conn->pending_err != ERR_OK) {
        out |= EPOLLERR;
    }

    return out;
}

static int xj380_provider_set_flags(void *socket_ptr, uint64_t flags)
{
    xj380_lwip_socket_t *socket = (xj380_lwip_socket_t *)socket_ptr;
    if (socket == NULL) {
        return -EINVAL;
    }

    netconn_set_nonblocking(socket->conn, (flags & O_NONBLOCK) != 0);
    return 0;
}

static socket_provider_t g_socket_provider = {
    .create    = xj380_provider_create,
    .close     = xj380_provider_close,
    .read      = xj380_provider_read,
    .write     = xj380_provider_write,
    .bind      = xj380_provider_bind,
    .connect   = xj380_provider_connect,
    .listen    = xj380_provider_listen,
    .accept    = xj380_provider_accept,
    .shutdown  = xj380_provider_shutdown,
    .poll      = xj380_provider_poll,
    .set_flags = xj380_provider_set_flags,
};

static void xj380_update_net_state(void)
{
    if (!g_stack_ready) {
        return;
    }

    const ip4_addr_t *ip = netif_ip4_addr(&g_netif);
    if (!g_dhcp_ready_logged && !ip4_addr_isany_val(*ip)) {
        const ip4_addr_t *mask = netif_ip4_netmask(&g_netif);
        const ip4_addr_t *gw   = netif_ip4_gw(&g_netif);
        g_dhcp_ready_logged    = true;

        printk("netserver: dhcp lease %u.%u.%u.%u mask=%u.%u.%u.%u gw=%u.%u.%u.%u\n",
               ip4_addr1(ip), ip4_addr2(ip), ip4_addr3(ip), ip4_addr4(ip),
               ip4_addr1(mask), ip4_addr2(mask), ip4_addr3(mask), ip4_addr4(mask),
               ip4_addr1(gw), ip4_addr2(gw), ip4_addr3(gw), ip4_addr4(gw));
    }
}

static void xj380_poll_input(void)
{
    uint8_t rx_buffer[2048];

    while (true) {
        int frame_len = g_netdev->recv(g_netdev->desc, rx_buffer, sizeof(rx_buffer));
        if (frame_len <= 0) {
            return;
        }

        if (g_rx_log_count < 24 && frame_len >= 14) {
            uint16_t eth_type = ((uint16_t)rx_buffer[12] << 8) | rx_buffer[13];
            printk("netserver: rx frame len=%u type=0x%04x dst=%02x:%02x:%02x:%02x:%02x:%02x src=%02x:%02x:%02x:%02x:%02x:%02x netif_flags=0x%x hwaddr_len=%u\n",
                   (unsigned int)frame_len,
                   (unsigned int)eth_type,
                   rx_buffer[0], rx_buffer[1], rx_buffer[2], rx_buffer[3], rx_buffer[4], rx_buffer[5],
                   rx_buffer[6], rx_buffer[7], rx_buffer[8], rx_buffer[9], rx_buffer[10], rx_buffer[11],
                   (unsigned int)g_netif.flags,
                   (unsigned int)g_netif.hwaddr_len);
            g_rx_log_count++;
        }

        struct pbuf *packet = pbuf_alloc(PBUF_RAW, (u16_t)frame_len, PBUF_POOL);
        if (packet == NULL) {
            continue;
        }

        size_t copied = 0;
        for (struct pbuf *cursor = packet; cursor != NULL; cursor = cursor->next) {
            memcpy(cursor->payload, rx_buffer + copied, cursor->len);
            copied += cursor->len;
        }

        if (g_netif.input(packet, &g_netif) != ERR_OK) {
            pbuf_free(packet);
        }
    }
}

static int xj380_setup_stack(void)
{
    ip4_addr_t ipaddr;
    ip4_addr_t netmask;
    ip4_addr_t gateway;
    bool       tcpip_ready = false;

    IP4_ADDR(&ipaddr, 10, 0, 2, 15);
    IP4_ADDR(&netmask, 255, 255, 255, 0);
    IP4_ADDR(&gateway, 10, 0, 2, 2);

    tcpip_init([](void *arg) { *(bool *)arg = true; }, &tcpip_ready);
    while (!tcpip_ready) {
        scheduler_yield();
    }

    LOCK_TCPIP_CORE();

    struct netif *added = netif_add(&g_netif, &ipaddr, &netmask, &gateway, g_netdev, xj380_netif_init, tcpip_input);
    if (added == NULL) {
        UNLOCK_TCPIP_CORE();
        return -EIO;
    }

    netif_set_default(&g_netif);
    netif_set_addr(&g_netif, &ipaddr, &netmask, &gateway);
    netif_set_link_up(&g_netif);
    netif_set_up(&g_netif);

    // The current lwIP port leaves admin/link flags and hwaddr_len inconsistent
    // after init; force the ethernet netif state so ARP/TCP can run.
    g_netif.hwaddr_len = ETH_HWADDR_LEN;
    memcpy(g_netif.hwaddr, g_netdev->mac, ETH_HWADDR_LEN);
    g_netif.flags |= (NETIF_FLAG_UP | NETIF_FLAG_LINK_UP);

    printk("netserver: netif ready flags=0x%x up=%u link=%u mtu=%u mac=%02x:%02x:%02x:%02x:%02x:%02x\n",
           (unsigned int)g_netif.flags,
           (unsigned int)netif_is_up(&g_netif),
           (unsigned int)netif_is_link_up(&g_netif),
           (unsigned int)g_netif.mtu,
           g_netif.hwaddr[0], g_netif.hwaddr[1], g_netif.hwaddr[2],
           g_netif.hwaddr[3], g_netif.hwaddr[4], g_netif.hwaddr[5]);
    printk("netserver: static ip %u.%u.%u.%u mask=%u.%u.%u.%u gw=%u.%u.%u.%u\n",
           ip4_addr1(&ipaddr), ip4_addr2(&ipaddr), ip4_addr3(&ipaddr), ip4_addr4(&ipaddr),
           ip4_addr1(&netmask), ip4_addr2(&netmask), ip4_addr3(&netmask), ip4_addr4(&netmask),
           ip4_addr1(&gateway), ip4_addr2(&gateway), ip4_addr3(&gateway), ip4_addr4(&gateway));

    UNLOCK_TCPIP_CORE();
    g_dhcp_ready_logged = true;

    g_stack_ready = true;
    return 0;
}

static void netserver_thread(uint64_t arg)
{
    (void)arg;
    printk("netserver: worker started\n");

    while (g_netdev == NULL) {
        g_netdev = get_default_netdev();
        if (g_netdev == NULL) {
            delay_ms_hp(100);
        }
    }

    printk("netserver: netdev ready, mac=%02x:%02x:%02x:%02x:%02x:%02x\n",
           g_netdev->mac[0], g_netdev->mac[1], g_netdev->mac[2],
           g_netdev->mac[3], g_netdev->mac[4], g_netdev->mac[5]);

    int ret = xj380_setup_stack();
    if (ret != 0) {
        printk("netserver: init failed: %d\n", ret);
        return;
    }

    while (true) {
        xj380_poll_input();
        xj380_update_net_state();
        delay_ms_hp(1);
    }
}

extern "C"
__attribute__((used)) __attribute__((visibility("default"))) int dlstart(void)
{
    return 0;
}

extern "C"
__attribute__((used)) __attribute__((visibility("default"))) int dlmain(void)
{
    if (!g_provider_registered) {
        int ret = socket_provider_register(&g_socket_provider);
        if (ret < 0) {
            printk("netserver: provider register failed: %d\n", ret);
            return ret;
        }
        g_provider_registered = true;
    }

    if (g_worker_started) {
        printk("netserver: dlmain ignored, worker already started\n");
        return 0;
    }

    g_worker_started = true;
    size_t tid = create_kernel_thread((void *)netserver_thread, NULL, (char *)"netserver", NULL);
    return tid == 0 ? -EIO : 0;
}
