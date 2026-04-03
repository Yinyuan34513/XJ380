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
#include "lwip/dns.h"
#include "lwip/etharp.h"
#include "lwip/ip4_addr.h"
#include "lwip/netif.h"
#include "lwip/netifapi.h"
#include "lwip/pbuf.h"
#include "lwip/prot/dhcp.h"
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
static bool         g_dhcp_started        = false;
static bool         g_dhcp_ready_logged   = false;
static bool         g_dns_manual_override = false;
static uint32_t     g_rx_log_count        = 0;
static uint32_t     g_tx_log_count        = 0;
static uint32_t     g_connect_log_count   = 0;
static uint32_t     g_write_log_count     = 0;
static uint32_t     g_dns_manual_addr     = 0;
static uint8_t      g_last_dhcp_state     = 0xff;

typedef struct xj380_dns_query {
    volatile int done;
    volatile int status;
    ip_addr_t     addr;
} xj380_dns_query_t;

static int xj380_err_to_errno(err_t err)
{
    if (err == ERR_OK) {
        return 0;
    }
    return -err_to_errno(err);
}

static uint8_t xj380_current_dhcp_state(void)
{
    struct dhcp *dhcp = netif_dhcp_data(&g_netif);
    return dhcp != NULL ? dhcp->state : DHCP_STATE_OFF;
}

static const char *xj380_dhcp_state_name(uint8_t state)
{
    switch (state) {
    case DHCP_STATE_OFF:
        return "off";
    case DHCP_STATE_REQUESTING:
        return "requesting";
    case DHCP_STATE_INIT:
        return "init";
    case DHCP_STATE_REBOOTING:
        return "rebooting";
    case DHCP_STATE_REBINDING:
        return "rebinding";
    case DHCP_STATE_RENEWING:
        return "renewing";
    case DHCP_STATE_SELECTING:
        return "selecting";
    case DHCP_STATE_INFORMING:
        return "informing";
    case DHCP_STATE_CHECKING:
        return "checking";
    case DHCP_STATE_PERMANENT:
        return "permanent";
    case DHCP_STATE_BOUND:
        return "bound";
    case DHCP_STATE_RELEASING:
        return "releasing";
    case DHCP_STATE_BACKING_OFF:
        return "backing-off";
    default:
        return "unknown";
    }
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

static void xj380_dns_found_cb(const char *name, const ip_addr_t *ipaddr, void *arg)
{
    xj380_dns_query_t *query = (xj380_dns_query_t *)arg;
    if (query == NULL) {
        return;
    }

    if (ipaddr == NULL) {
        query->status = -EHOSTUNREACH;
        query->done   = 1;
        printk("netserver: dns callback name=%s status=not-found\n", name ? name : "(null)");
        return;
    }

    query->addr = *ipaddr;
    if (!IP_IS_V4(ipaddr)) {
        query->status = -EAFNOSUPPORT;
        query->done   = 1;
        printk("netserver: dns callback name=%s family=non-ipv4\n", name ? name : "(null)");
        return;
    }

    const ip4_addr_t *ip4 = ip_2_ip4(ipaddr);
    query->status = 0;
    query->done   = 1;
    printk("netserver: dns callback name=%s addr=%u.%u.%u.%u\n",
           name ? name : "(null)",
           ip4_addr1(ip4), ip4_addr2(ip4), ip4_addr3(ip4), ip4_addr4(ip4));
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
    printk("netserver: socket create domain=%d type=%d protocol=%d stack_ready=%d\n",
           domain, type, protocol, (int)g_stack_ready);

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
        printk("netserver: netconn_new failed\n");
        return -ENOMEM;
    }
    printk("netserver: netconn_new ok conn=%p\n", conn);

    xj380_lwip_socket_t *socket = (xj380_lwip_socket_t *)calloc(1, sizeof(xj380_lwip_socket_t));
    if (socket == NULL) {
        netconn_delete(conn);
        return -ENOMEM;
    }

    socket->conn = conn;
    *out_socket  = socket;
    printk("netserver: socket create ok socket=%p\n", socket);
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

    if (g_write_log_count < 16) {
        printk("netserver: write size=%u flags=0x%x state=%d pending_err=%d\n",
               (unsigned int)size,
               (unsigned int)flags,
               (int)socket->conn->state,
               (int)socket->conn->pending_err);
        g_write_log_count++;
    }

    err_t err = netconn_write_partly(socket->conn, buffer, size, api_flags, &written);
    if (err != ERR_OK) {
        if (g_write_log_count < 24) {
            printk("netserver: write err=%d written=%u state=%d pending_err=%d\n",
                   (int)err,
                   (unsigned int)written,
                   (int)socket->conn->state,
                   (int)socket->conn->pending_err);
            g_write_log_count++;
        }
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

    if (g_connect_log_count < 16) {
        const ip4_addr_t *ip4 = ip_2_ip4(&ip);
        printk("netserver: connect dst=%u.%u.%u.%u:%u flags=0x%x state=%d pending_err=%d\n",
               ip4_addr1(ip4), ip4_addr2(ip4), ip4_addr3(ip4), ip4_addr4(ip4),
               (unsigned int)port,
               (unsigned int)netconn_is_nonblocking(socket->conn),
               (int)socket->conn->state,
               (int)socket->conn->pending_err);
        g_connect_log_count++;
    }

    err_t err = netconn_connect(socket->conn, &ip, port);

    if (g_connect_log_count < 24) {
        printk("netserver: connect ret=%d state=%d pending_err=%d\n",
               (int)err,
               (int)socket->conn->state,
               (int)socket->conn->pending_err);
        g_connect_log_count++;
    }

    return xj380_err_to_errno(err);
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
        if (socket->conn->state != NETCONN_CONNECT && socket->conn->pending_err == ERR_OK) {
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

static int xj380_provider_resolve_ipv4(const char *hostname, uint32_t *out_addr)
{
    if (hostname == NULL || out_addr == NULL || hostname[0] == '\0') {
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

    ip_addr_t resolved_addr;
    if (ipaddr_aton(hostname, &resolved_addr)) {
        if (!IP_IS_V4(&resolved_addr)) {
            return -EAFNOSUPPORT;
        }
        *out_addr = ip_2_ip4(&resolved_addr)->addr;
        return 0;
    }

    printk("netserver: dns resolve start host=%s\n", hostname);

    xj380_dns_query_t query;
    memset(&query, 0, sizeof(query));

    LOCK_TCPIP_CORE();
    err_t err = dns_gethostbyname_addrtype(hostname, &resolved_addr, xj380_dns_found_cb, &query, LWIP_DNS_ADDRTYPE_IPV4);
    UNLOCK_TCPIP_CORE();

    if (err != ERR_OK) {
        if (err != ERR_INPROGRESS) {
            printk("netserver: dns resolve host=%s err=%d\n", hostname, (int)err);
            return xj380_err_to_errno(err);
        }

        uint32_t start_ms = sys_now();
        while (!query.done) {
            if ((uint32_t)(sys_now() - start_ms) >= 5000U) {
                printk("netserver: dns resolve timeout host=%s\n", hostname);
                return -ETIMEDOUT;
            }
            scheduler_yield();
        }

        if (query.status < 0) {
            printk("netserver: dns resolve failed host=%s status=%d\n", hostname, query.status);
            return query.status;
        }
        resolved_addr = query.addr;
    }
    if (!IP_IS_V4(&resolved_addr)) {
        return -EAFNOSUPPORT;
    }

    *out_addr = ip_2_ip4(&resolved_addr)->addr;
    const ip4_addr_t *ip4 = ip_2_ip4(&resolved_addr);
    printk("netserver: dns resolve done host=%s addr=%u.%u.%u.%u\n",
           hostname,
           ip4_addr1(ip4), ip4_addr2(ip4), ip4_addr3(ip4), ip4_addr4(ip4));
    return 0;
}

static int xj380_provider_set_dns_server(uint32_t addr)
{
    if (addr == 0) {
        g_dns_manual_override = false;
        g_dns_manual_addr     = 0;
        return 0;
    }

    ip4_addr_t ip4;
    ip4.addr = addr;

    ip_addr_t dns_addr;
    ip_addr_copy_from_ip4(dns_addr, ip4);
    dns_setserver(0, &dns_addr);
    g_dns_manual_override = true;
    g_dns_manual_addr     = addr;
    return 0;
}

static int xj380_provider_get_dns_server(uint32_t *out_addr)
{
    if (out_addr == NULL) {
        return -EINVAL;
    }

    const ip_addr_t *dns_addr = dns_getserver(0);
    if (dns_addr == NULL || !IP_IS_V4(dns_addr)) {
        return -ENODATA;
    }

    *out_addr = ip_2_ip4(dns_addr)->addr;
    return 0;
}

static int xj380_provider_get_netinfo(socket_netinfo_t *out_info)
{
    if (out_info == NULL) {
        return -EINVAL;
    }

    memset(out_info, 0, sizeof(*out_info));

    if (g_netdev != NULL) {
        memcpy(out_info->mac, g_netdev->mac, sizeof(out_info->mac));
        out_info->mtu = g_netdev->mtu;
    }

    out_info->ifname[0] = 'e';
    out_info->ifname[1] = '0';
    out_info->ifname[2] = '\0';
    out_info->stack_ready = g_stack_ready ? 1 : 0;
    out_info->ipv4_method = g_dhcp_started ? SOCKET_IPV4_METHOD_DHCP : SOCKET_IPV4_METHOD_NONE;

    if (!g_stack_ready) {
        return 0;
    }

    out_info->link_up = (netif_is_up(&g_netif) && netif_is_link_up(&g_netif)) ? 1 : 0;
    out_info->mtu     = g_netif.mtu;
    out_info->dhcp_state = xj380_current_dhcp_state();
    memcpy(out_info->mac, g_netif.hwaddr, sizeof(out_info->mac));

    const ip4_addr_t *ip = netif_ip4_addr(&g_netif);
    if (ip != NULL && !ip4_addr_isany_val(*ip)) {
        out_info->ip_ready     = 1;
        out_info->ipv4_addr    = ip->addr;
        out_info->ipv4_netmask = netif_ip4_netmask(&g_netif)->addr;
        out_info->ipv4_gateway = netif_ip4_gw(&g_netif)->addr;
    }

    uint32_t dns_addr = 0;
    if (xj380_provider_get_dns_server(&dns_addr) == 0) {
        out_info->dns_server = dns_addr;
    }

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
    .resolve_ipv4 = xj380_provider_resolve_ipv4,
    .set_dns_server = xj380_provider_set_dns_server,
    .get_dns_server = xj380_provider_get_dns_server,
    .get_netinfo = xj380_provider_get_netinfo,
};

static void xj380_update_net_state(void)
{
    if (!g_stack_ready) {
        return;
    }

    uint8_t dhcp_state = xj380_current_dhcp_state();
    if (dhcp_state != g_last_dhcp_state) {
        g_last_dhcp_state = dhcp_state;
        printk("netserver: dhcp state -> %s (%u)\n",
               xj380_dhcp_state_name(dhcp_state), (unsigned int)dhcp_state);
        if (dhcp_state != DHCP_STATE_BOUND && dhcp_state != DHCP_STATE_RENEWING &&
            dhcp_state != DHCP_STATE_REBINDING) {
            g_dhcp_ready_logged = false;
        }
    }

    if (g_dns_manual_override) {
        const ip_addr_t *dns_addr = dns_getserver(0);
        uint32_t current_dns = 0;
        if (dns_addr != NULL && IP_IS_V4(dns_addr)) {
            current_dns = ip_2_ip4(dns_addr)->addr;
        }
        if (current_dns != g_dns_manual_addr) {
            ip4_addr_t ip4;
            ip4.addr = g_dns_manual_addr;
            ip_addr_t dns_addr_out;
            ip_addr_copy_from_ip4(dns_addr_out, ip4);
            dns_setserver(0, &dns_addr_out);
        }
    }

    const ip4_addr_t *ip = netif_ip4_addr(&g_netif);
    if (!g_dhcp_ready_logged && dhcp_supplied_address(&g_netif) && !ip4_addr_isany_val(*ip)) {
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
    bool       tcpip_ready = false;

    tcpip_init([](void *arg) { *(bool *)arg = true; }, &tcpip_ready);
    while (!tcpip_ready) {
        scheduler_yield();
    }

    LOCK_TCPIP_CORE();

    struct netif *added = netif_add(&g_netif, NULL, NULL, NULL, g_netdev, xj380_netif_init, tcpip_input);
    if (added == NULL) {
        UNLOCK_TCPIP_CORE();
        return -EIO;
    }

    netif_set_default(&g_netif);
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

    err_t dhcp_err = dhcp_start(&g_netif);
    if (dhcp_err != ERR_OK) {
        printk("netserver: dhcp_start failed: %d\n", (int)dhcp_err);
        UNLOCK_TCPIP_CORE();
        return xj380_err_to_errno(dhcp_err);
    }

    if (g_dns_manual_override && g_dns_manual_addr != 0) {
        ip4_addr_t ip4;
        ip4.addr = g_dns_manual_addr;
        ip_addr_t dns_addr;
        ip_addr_copy_from_ip4(dns_addr, ip4);
        dns_setserver(0, &dns_addr);
    }

    printk("netserver: dhcp client started on %c%c\n", g_netif.name[0], g_netif.name[1]);

    UNLOCK_TCPIP_CORE();

    g_dhcp_started      = true;
    g_dhcp_ready_logged = false;
    g_last_dhcp_state   = 0xff;
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
