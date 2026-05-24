#pragma once

#include "../stdint.h"
#include "sys/types.h"
#include "sys/uio.h"

typedef uint16_t sa_family_t;
typedef uint16_t in_port_t;
typedef uint32_t socklen_t;

#ifdef __cplusplus
extern "C" {
#endif

#define AF_UNSPEC 0
#define AF_UNIX   1
#define AF_LOCAL  AF_UNIX
#define AF_INET   2
#define AF_INET6  10

#define SOCK_STREAM 1
#define SOCK_DGRAM  2
#define SOCK_TYPE_MASK 0xf
#define SOCK_CLOEXEC 02000000
#define SOCK_NONBLOCK 00004000

#define SOL_SOCKET 1
#define SCM_RIGHTS 0x01

#define MSG_CMSG_CLOEXEC 0x40000000U

#define SHUT_RD   0
#define SHUT_WR   1
#define SHUT_RDWR 2

struct in_addr {
    uint32_t s_addr;
};

struct in6_addr {
    uint8_t s6_addr[16];
};

struct sockaddr {
    sa_family_t sa_family;
    char        sa_data[14];
};

struct sockaddr_in {
    sa_family_t     sin_family;
    in_port_t       sin_port;
    struct in_addr  sin_addr;
    unsigned char   sin_zero[8];
};

struct sockaddr_in6 {
    sa_family_t     sin6_family;
    in_port_t       sin6_port;
    uint32_t        sin6_flowinfo;
    struct in6_addr sin6_addr;
    uint32_t        sin6_scope_id;
};

struct msghdr {
    void         *msg_name;
    socklen_t    msg_namelen;
    struct iovec *msg_iov;
    size_t       msg_iovlen;
    void         *msg_control;
    size_t       msg_controllen;
    int          msg_flags;
};

struct cmsghdr {
    size_t cmsg_len;
    int    cmsg_level;
    int    cmsg_type;
};

#define CMSG_ALIGN(len) (((size_t)(len) + sizeof(size_t) - 1) & ~(sizeof(size_t) - 1))
#define CMSG_SPACE(len) (CMSG_ALIGN(sizeof(struct cmsghdr)) + CMSG_ALIGN(len))
#define CMSG_LEN(len)   (CMSG_ALIGN(sizeof(struct cmsghdr)) + (size_t)(len))
#define CMSG_DATA(cmsg) ((unsigned char *)(cmsg) + CMSG_ALIGN(sizeof(struct cmsghdr)))
#define CMSG_FIRSTHDR(mhdr) \
    ((mhdr) != NULL && (mhdr)->msg_control != NULL && (mhdr)->msg_controllen >= sizeof(struct cmsghdr) \
         ? (struct cmsghdr *)(mhdr)->msg_control \
         : (struct cmsghdr *)0)
#define CMSG_NXTHDR(mhdr, cmsg) \
    ((((unsigned char *)(cmsg) + CMSG_ALIGN((cmsg)->cmsg_len) + sizeof(struct cmsghdr)) > \
      ((unsigned char *)(mhdr)->msg_control + (mhdr)->msg_controllen)) \
         ? (struct cmsghdr *)0 \
         : (struct cmsghdr *)((unsigned char *)(cmsg) + CMSG_ALIGN((cmsg)->cmsg_len)))

static inline uint16_t htons(uint16_t value)
{
    return (uint16_t)((value << 8) | (value >> 8));
}

static inline uint16_t ntohs(uint16_t value)
{
    return htons(value);
}

static inline uint32_t htonl(uint32_t value)
{
    return ((value & 0x000000ffU) << 24) |
           ((value & 0x0000ff00U) << 8) |
           ((value & 0x00ff0000U) >> 8) |
           ((value & 0xff000000U) >> 24);
}

static inline uint32_t ntohl(uint32_t value)
{
    return htonl(value);
}

int socket(int domain, int type, int protocol);
int socketpair(int domain, int type, int protocol, int sv[2]);
int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
int listen(int sockfd, int backlog);
int accept(int sockfd, struct sockaddr *addr, socklen_t *addrlen);
int shutdown(int sockfd, int how);
ssize_t sendmsg(int sockfd, const struct msghdr *msg, int flags);
ssize_t recvmsg(int sockfd, struct msghdr *msg, int flags);

#ifdef __cplusplus
}
#endif
