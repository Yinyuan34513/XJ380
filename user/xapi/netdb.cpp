#include "./include/x3api.h"
#include "./include/libsys.h"
#include "./include/xposix/netdb.h"

static int xnetdb_parse_ipv4(const char *text, uint32_t *out_addr)
{
    uint32_t parts[4] = {0, 0, 0, 0};
    int      part     = 0;
    uint32_t value    = 0;

    if (text == NULL || out_addr == NULL) {
        return 0;
    }

    for (const char *p = text;; ++p) {
        char ch = *p;
        if (ch >= '0' && ch <= '9') {
            value = value * 10 + (uint32_t)(ch - '0');
            if (value > 255) {
                return 0;
            }
        } else if (ch == '.' || ch == '\0') {
            if (part >= 4) {
                return 0;
            }
            parts[part++] = value;
            value         = 0;
            if (ch == '\0') {
                break;
            }
        } else {
            return 0;
        }
    }

    if (part != 4) {
        return 0;
    }

    *out_addr = htonl((parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3]);
    return 1;
}

static int xnetdb_parse_service(const char *service, uint16_t *out_port)
{
    if (out_port == NULL) {
        return EAI_SERVICE;
    }
    if (service == NULL || service[0] == '\0') {
        *out_port = 0;
        return 0;
    }

    long port = strtol(service, NULL, 10);
    if (port < 0 || port > 65535) {
        return EAI_SERVICE;
    }

    *out_port = (uint16_t)port;
    return 0;
}

static int xnetdb_query_dnsfs(const char *hostname, uint32_t *out_addr)
{
    static char dnsfs_path[] = "/run/dns/resolve";
    char        response[64];

    if (hostname == NULL || hostname[0] == '\0' || out_addr == NULL) {
        return EAI_NONAME;
    }

    int fd = (int)enter_syscall(SYS_OPEN, (uint64_t)dnsfs_path, 0, 0, 0, 0, 0);
    if (fd < 0) {
        return EAI_FAIL;
    }

    int name_len = (int)strlen(hostname);
    int wrote = write(fd, (char *)hostname, (uint64_t)name_len);
    if (wrote != name_len) {
        close(fd);
        return EAI_FAIL;
    }

    int got = read(fd, response, sizeof(response) - 1);
    close(fd);
    if (got <= 0) {
        return EAI_NONAME;
    }

    response[got] = '\0';
    while (got > 0 && (response[got - 1] == '\n' || response[got - 1] == '\r' || response[got - 1] == ' ' || response[got - 1] == '\t')) {
        response[--got] = '\0';
    }

    if (!xnetdb_parse_ipv4(response, out_addr)) {
        return EAI_FAIL;
    }
    return 0;
}

int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res)
{
    if (res == NULL) {
        return EAI_FAIL;
    }
    *res = NULL;

    if (hints != NULL) {
        if (hints->ai_family != 0 && hints->ai_family != AF_INET && hints->ai_family != AF_UNSPEC) {
            return EAI_FAMILY;
        }
        if (hints->ai_socktype != 0 && hints->ai_socktype != SOCK_STREAM) {
            return EAI_SOCKTYPE;
        }
        if ((hints->ai_flags & ~(AI_PASSIVE | AI_CANONNAME | AI_NUMERICHOST)) != 0) {
            return EAI_BADFLAGS;
        }
    }

    uint16_t port = 0;
    int service_status = xnetdb_parse_service(service, &port);
    if (service_status != 0) {
        return service_status;
    }

    uint32_t addr = 0;
    if (node == NULL || node[0] == '\0') {
        addr = (hints != NULL && (hints->ai_flags & AI_PASSIVE)) ? 0 : htonl(0x7f000001U);
    } else if (xnetdb_parse_ipv4(node, &addr)) {
        // numeric literal
    } else {
        if (hints != NULL && (hints->ai_flags & AI_NUMERICHOST)) {
            return EAI_NONAME;
        }
        int dns_status = xnetdb_query_dnsfs(node, &addr);
        if (dns_status != 0) {
            return dns_status;
        }
    }

    struct addrinfo *info = (struct addrinfo *)calloc(1, sizeof(struct addrinfo));
    struct sockaddr_in *addr_in = (struct sockaddr_in *)calloc(1, sizeof(struct sockaddr_in));
    if (info == NULL || addr_in == NULL) {
        free(info);
        free(addr_in);
        return EAI_MEMORY;
    }

    addr_in->sin_family      = AF_INET;
    addr_in->sin_port        = htons(port);
    addr_in->sin_addr.s_addr = addr;

    info->ai_flags    = hints ? hints->ai_flags : 0;
    info->ai_family   = AF_INET;
    info->ai_socktype = hints && hints->ai_socktype ? hints->ai_socktype : SOCK_STREAM;
    info->ai_protocol = hints ? hints->ai_protocol : 0;
    info->ai_addrlen  = sizeof(struct sockaddr_in);
    info->ai_addr     = (struct sockaddr *)addr_in;
    info->ai_next     = NULL;

    if (node != NULL && hints != NULL && (hints->ai_flags & AI_CANONNAME)) {
        info->ai_canonname = (char *)malloc(strlen(node) + 1);
        if (info->ai_canonname == NULL) {
            free(addr_in);
            free(info);
            return EAI_MEMORY;
        }
        strcpy(info->ai_canonname, node);
    }

    *res = info;
    return 0;
}

void freeaddrinfo(struct addrinfo *res)
{
    while (res != NULL) {
        struct addrinfo *next = res->ai_next;
        free(res->ai_addr);
        free(res->ai_canonname);
        free(res);
        res = next;
    }
}

const char *gai_strerror(int errcode)
{
    switch (errcode) {
    case 0: return "success";
    case EAI_BADFLAGS: return "bad flags";
    case EAI_NONAME: return "name or service not known";
    case EAI_AGAIN: return "temporary failure";
    case EAI_FAIL: return "non-recoverable failure";
    case EAI_FAMILY: return "address family not supported";
    case EAI_SOCKTYPE: return "socket type not supported";
    case EAI_SERVICE: return "service not supported";
    case EAI_MEMORY: return "out of memory";
    default: return "unknown error";
    }
}

struct hostent *gethostbyname(const char *name)
{
    static struct hostent host;
    static char          *aliases[1] = {NULL};
    static char          *addr_list[2] = {NULL, NULL};
    static char           canonical_name[256];
    static struct in_addr addr;

    if (name == NULL || name[0] == '\0') {
        return NULL;
    }

    if (!xnetdb_parse_ipv4(name, &addr.s_addr)) {
        if (xnetdb_query_dnsfs(name, &addr.s_addr) != 0) {
            return NULL;
        }
    }

    strncpy(canonical_name, name, sizeof(canonical_name) - 1);
    canonical_name[sizeof(canonical_name) - 1] = '\0';

    host.h_name      = canonical_name;
    host.h_aliases   = aliases;
    host.h_addrtype  = AF_INET;
    host.h_length    = sizeof(struct in_addr);
    addr_list[0]     = (char *)&addr;
    host.h_addr_list = addr_list;
    return &host;
}
