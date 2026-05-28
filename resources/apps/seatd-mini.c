/* Minimal seatd for XJ380 — just enough for Weston's drm-backend */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/stat.h>

#define SEATD_SOCK "/run/seatd.sock"
#define MAX_CLIENTS 8

/* Protocol messages (from libseat/backend/seatd.c) */
enum {
    SEATD_MSG_OPEN_SEAT      = 1,
    SEATD_MSG_CLOSE_SEAT     = 2,
    SEATD_MSG_OPEN_DEVICE    = 3,
    SEATD_MSG_CLOSE_DEVICE   = 4,
    SEATD_MSG_SWITCH_SESSION = 5,
    SEATD_MSG_PING           = 6,
};

struct msg_header {
    uint16_t type;
    uint16_t len;
};

static int send_fd(int sock, int fd, void *data, size_t len) {
    struct msghdr msg = {0};
    struct iovec iov = { .iov_base = data, .iov_len = len };
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;

    char cbuf[CMSG_SPACE(sizeof(int))];
    msg.msg_control = cbuf;
    msg.msg_controllen = sizeof(cbuf);

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type  = SCM_RIGHTS;
    cmsg->cmsg_len   = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(cmsg), &fd, sizeof(int));
    msg.msg_controllen = CMSG_SPACE(sizeof(int));

    return sendmsg(sock, &msg, 0);
}

int main(void) {
    /* chdir to / so we don't hold a mount point */
    chdir("/");

    /* Fork to background */
    if (fork() != 0) return 0;
    setsid();

    unlink(SEATD_SOCK);
    int srv = socket(AF_UNIX, SOCK_STREAM, 0);
    if (srv < 0) return 1;

    struct sockaddr_un addr = { .sun_family = AF_UNIX };
    strcpy(addr.sun_path, SEATD_SOCK);
    if (bind(srv, (struct sockaddr *)&addr, sizeof(addr)) < 0) return 1;
    if (listen(srv, 5) < 0) return 1;
    chmod(SEATD_SOCK, 0666);

    for (;;) {
        int client = accept(srv, NULL, NULL);
        if (client < 0) continue;

        for (;;) {
            struct msg_header hdr;
            ssize_t n = recv(client, &hdr, sizeof(hdr), MSG_WAITALL);
            if (n <= 0) break;

            if (hdr.type == SEATD_MSG_OPEN_SEAT) {
                /* Just accept — send version + seat name */
                uint8_t buf[16] = {0, 0, 0, 0,  /* version = 0 */
                                   's','e','a','t','0', 0}; /* seat name */
                uint16_t resp_type = SEATD_MSG_OPEN_SEAT;
                send_fd(client, -1, &resp_type, 2);
                send(client, buf, sizeof(buf), 0);
            } else if (hdr.type == SEATD_MSG_CLOSE_SEAT) {
                break;
            } else if (hdr.type == SEATD_MSG_OPEN_DEVICE) {
                char path[256] = {0};
                size_t plen = hdr.len < sizeof(path) - 1 ? hdr.len : sizeof(path) - 1;
                recv(client, path, plen, MSG_WAITALL);

                int devfd = open(path, O_RDWR | O_CLOEXEC);
                uint16_t resp_type = SEATD_MSG_OPEN_DEVICE;
                struct { int32_t dev_id; int32_t fd; } resp = { .dev_id = 1, .fd = devfd };
                send_fd(client, devfd >= 0 ? devfd : -1, &resp_type, 2);
                if (devfd >= 0) send(client, &resp, sizeof(resp), 0);
            } else if (hdr.type == SEATD_MSG_CLOSE_DEVICE) {
                int32_t dev_id;
                recv(client, &dev_id, sizeof(dev_id), MSG_WAITALL);
                /* ignore — devices get closed with the seat */
            } else if (hdr.type == SEATD_MSG_PING) {
                uint16_t resp_type = SEATD_MSG_PING;
                send(client, &resp_type, 2, 0);
            } else {
                /* Unknown — drain payload */
                char drain[256];
                while (hdr.len > 0) {
                    size_t chunk = hdr.len > sizeof(drain) ? sizeof(drain) : hdr.len;
                    recv(client, drain, chunk, 0);
                    hdr.len -= (uint16_t)chunk;
                }
            }
        }
        close(client);
    }
}
