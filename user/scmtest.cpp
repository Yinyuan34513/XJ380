#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/uio.h>
#include <unistd.h>
#include <poll.h>
#include "xapi/include/xposix/socket.h"

#ifndef FD_CLOEXEC
#define FD_CLOEXEC 1
#endif

static int g_failures = 0;

static void out(const char *s)
{
    write(1, s, strlen(s));
}

static void log_result(const char *name, int ok)
{
    if (ok) {
        out("ok ");
        out(name);
        out("\n");
    } else {
        out("FAIL ");
        out(name);
        out("\n");
        g_failures++;
    }
}

static void close_if_valid(int fd)
{
    if (fd >= 0) close(fd);
}

static int send_fd_with_payload(int sockfd, int fd_to_send, const char *payload)
{
    char control[CMSG_SPACE(sizeof(int))];
    memset(control, 0, sizeof(control));

    struct iovec iov = {};
    iov.iov_base = (void *)payload;
    iov.iov_len = strlen(payload);

    struct msghdr msg = {};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control;
    msg.msg_controllen = sizeof(control);

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg == NULL) return -1;
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    *(int *)CMSG_DATA(cmsg) = fd_to_send;
    msg.msg_controllen = CMSG_SPACE(sizeof(int));

    return (int)sendmsg(sockfd, &msg, 0);
}

static int send_payload(int sockfd, const char *payload)
{
    struct iovec iov = {};
    iov.iov_base = (void *)payload;
    iov.iov_len = strlen(payload);

    struct msghdr msg = {};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    return (int)sendmsg(sockfd, &msg, 0);
}

static int recv_fd_with_payload(int sockfd, char *payload_buf, size_t payload_buf_size, int flags, int *out_fd)
{
    char control[CMSG_SPACE(sizeof(int))];
    memset(control, 0, sizeof(control));

    struct iovec iov = {};
    iov.iov_base = payload_buf;
    iov.iov_len = payload_buf_size;

    struct msghdr msg = {};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control;
    msg.msg_controllen = sizeof(control);

    int ret = (int)recvmsg(sockfd, &msg, flags);
    if (ret < 0) return ret;

    if ((size_t)ret < payload_buf_size) payload_buf[ret] = '\0';
    if (out_fd != NULL) *out_fd = -1;

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg != NULL &&
        cmsg->cmsg_level == SOL_SOCKET &&
        cmsg->cmsg_type == SCM_RIGHTS &&
        cmsg->cmsg_len >= CMSG_LEN(sizeof(int))) {
        if (out_fd != NULL) *out_fd = *(int *)CMSG_DATA(cmsg);
    }
    return ret;
}

static void test_zero_len_dgram_passcred()
{
    int sv[2] = {-1, -1};
    int sp_ret = socketpair(AF_UNIX, SOCK_DGRAM, 0, sv);
    log_result("socketpair(AF_UNIX, SOCK_DGRAM)", sp_ret == 0);
    if (sp_ret != 0) return;

    int one = 1;
    log_result("setsockopt SO_PASSCRED", setsockopt(sv[1], SOL_SOCKET, SO_PASSCRED, &one, sizeof(one)) == 0);

    struct iovec send_iov = {};
    struct msghdr send_msg = {};
    send_msg.msg_iov = &send_iov;
    send_msg.msg_iovlen = 1;
    int send_ret = (int)sendmsg(sv[0], &send_msg, 0);
    log_result("sendmsg zero-length datagram", send_ret == 0);

    struct pollfd pfd = {};
    pfd.fd = sv[1];
    pfd.events = POLLIN;
    int poll_ret = poll(&pfd, 1, 0);
    log_result("poll zero-length datagram readable", poll_ret == 1 && (pfd.revents & POLLIN));

    char control[CMSG_SPACE(sizeof(struct ucred))];
    memset(control, 0, sizeof(control));

    struct iovec recv_iov = {};
    struct msghdr recv_msg = {};
    recv_msg.msg_iov = &recv_iov;
    recv_msg.msg_iovlen = 1;
    recv_msg.msg_control = control;
    recv_msg.msg_controllen = sizeof(control);

    int recv_ret = (int)recvmsg(sv[1], &recv_msg, MSG_DONTWAIT);
    log_result("recvmsg zero-length datagram", recv_ret == 0);

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&recv_msg);
    bool has_cred = cmsg != NULL &&
                    cmsg->cmsg_level == SOL_SOCKET &&
                    cmsg->cmsg_type == SCM_CREDENTIALS &&
                    cmsg->cmsg_len >= CMSG_LEN(sizeof(struct ucred));
    log_result("recvmsg SCM_CREDENTIALS", has_cred);
    if (has_cred) {
        struct ucred *cred = (struct ucred *)CMSG_DATA(cmsg);
        log_result("SCM_CREDENTIALS pid valid", cred->pid == getpid());
    }

    close_if_valid(sv[0]);
    close_if_valid(sv[1]);
}

static void test_explicit_stream_credentials()
{
    int sv[2] = {-1, -1};
    int sp_ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    log_result("socketpair explicit SCM_CREDENTIALS", sp_ret == 0);
    if (sp_ret != 0) return;

    int one = 1;
    log_result("setsockopt stream SO_PASSCRED", setsockopt(sv[1], SOL_SOCKET, SO_PASSCRED, &one, sizeof(one)) == 0);

    const char payload[] = "cred";
    char control[CMSG_SPACE(sizeof(struct ucred))];
    memset(control, 0, sizeof(control));

    struct iovec iov = {};
    iov.iov_base = (void *)payload;
    iov.iov_len = sizeof(payload) - 1;

    struct msghdr msg = {};
    msg.msg_iov = &iov;
    msg.msg_iovlen = 1;
    msg.msg_control = control;
    msg.msg_controllen = sizeof(control);

    struct cmsghdr *cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg == NULL) {
        log_result("prepare explicit SCM_CREDENTIALS", 0);
        close_if_valid(sv[0]);
        close_if_valid(sv[1]);
        return;
    }

    struct ucred cred = {};
    cred.pid = getpid();
    cred.uid = getuid();
    cred.gid = getgid();
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_CREDENTIALS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(cred));
    memcpy(CMSG_DATA(cmsg), &cred, sizeof(cred));
    msg.msg_controllen = CMSG_SPACE(sizeof(cred));

    int send_ret = (int)sendmsg(sv[0], &msg, 0);
    log_result("sendmsg explicit SCM_CREDENTIALS", send_ret == (int)(sizeof(payload) - 1));

    char recv_buf[16];
    memset(recv_buf, 0, sizeof(recv_buf));
    char recv_control[CMSG_SPACE(sizeof(struct ucred))];
    memset(recv_control, 0, sizeof(recv_control));

    struct iovec recv_iov = {};
    recv_iov.iov_base = recv_buf;
    recv_iov.iov_len = sizeof(recv_buf) - 1;

    struct msghdr recv_msg = {};
    recv_msg.msg_iov = &recv_iov;
    recv_msg.msg_iovlen = 1;
    recv_msg.msg_control = recv_control;
    recv_msg.msg_controllen = sizeof(recv_control);

    int recv_ret = (int)recvmsg(sv[1], &recv_msg, 0);
    log_result("recvmsg explicit SCM_CREDENTIALS payload", recv_ret == (int)(sizeof(payload) - 1));
    log_result("explicit credential payload matches", strcmp(recv_buf, payload) == 0);

    struct cmsghdr *recv_cmsg = CMSG_FIRSTHDR(&recv_msg);
    bool has_cred = recv_cmsg != NULL &&
                    recv_cmsg->cmsg_level == SOL_SOCKET &&
                    recv_cmsg->cmsg_type == SCM_CREDENTIALS &&
                    recv_cmsg->cmsg_len >= CMSG_LEN(sizeof(struct ucred));
    log_result("recvmsg explicit SCM_CREDENTIALS", has_cred);
    if (has_cred) {
        struct ucred *received = (struct ucred *)CMSG_DATA(recv_cmsg);
        log_result("explicit SCM_CREDENTIALS pid valid", received->pid == getpid());
        log_result("explicit SCM_CREDENTIALS uid valid", received->uid == (int)getuid());
        log_result("explicit SCM_CREDENTIALS gid valid", received->gid == (int)getgid());
    }

    cmsg = CMSG_FIRSTHDR(&msg);
    if (cmsg != NULL) {
        ((struct ucred *)CMSG_DATA(cmsg))->pid = getpid() + 1;
        send_ret = (int)sendmsg(sv[0], &msg, 0);
        log_result("sendmsg forged SCM_CREDENTIALS rejected", send_ret < 0);
    }

    close_if_valid(sv[0]);
    close_if_valid(sv[1]);
}

static void test_stream_rights_packet_boundaries(int source_fd)
{
    int sv[2] = {-1, -1};
    int sp_ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    log_result("socketpair stream SCM_RIGHTS boundaries", sp_ret == 0);
    if (sp_ret != 0) return;

    log_result("send first packet with SCM_RIGHTS", send_fd_with_payload(sv[0], source_fd, "A") == 1);
    log_result("send middle packet without SCM_RIGHTS", send_payload(sv[0], "B") == 1);
    log_result("send second packet with SCM_RIGHTS", send_fd_with_payload(sv[0], source_fd, "C") == 1);

    char payload[16] = {};
    int first_fd = -1;
    int recv_ret = recv_fd_with_payload(sv[1], payload, sizeof(payload) - 1, 0, &first_fd);
    log_result("first recv stops before second SCM_RIGHTS", recv_ret == 2 && strcmp(payload, "AB") == 0);
    log_result("first recv has SCM_RIGHTS", first_fd >= 0);

    memset(payload, 0, sizeof(payload));
    int second_fd = -1;
    recv_ret = recv_fd_with_payload(sv[1], payload, sizeof(payload) - 1, 0, &second_fd);
    log_result("second recv preserves payload", recv_ret == 1 && strcmp(payload, "C") == 0);
    log_result("second recv preserves SCM_RIGHTS", second_fd >= 0);

    close_if_valid(first_fd);
    close_if_valid(second_fd);
    close_if_valid(sv[0]);
    close_if_valid(sv[1]);
}

static int scmtest_main_impl()
{
    out("scmtest: start\n");

    int data_fd = open("/scmtest.tmp", O_CREAT | O_RDWR | O_TRUNC, 0644);
    if (data_fd < 0) {
        out("scmtest: open failed\n");
        return 1;
    }

    const char file_payload[] = "fd-pass-through-works";
    int wrote = (int)write(data_fd, file_payload, sizeof(file_payload) - 1);
    log_result("prepare write source file", wrote == (int)(sizeof(file_payload) - 1));
    log_result("rewind source fd", lseek(data_fd, 0, SEEK_SET) == 0);

    int sv[2] = {-1, -1};
    int sp_ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sv);
    log_result("socketpair(AF_UNIX, SOCK_STREAM)", sp_ret == 0);
    if (sp_ret != 0) {
        close_if_valid(data_fd);
        return 1;
    }

    test_zero_len_dgram_passcred();
    test_explicit_stream_credentials();
    test_stream_rights_packet_boundaries(data_fd);

    const char msg1[] = "hello-fd";
    int send_ret = send_fd_with_payload(sv[0], data_fd, msg1);
    log_result("sendmsg SCM_RIGHTS", send_ret == (int)strlen(msg1));

    char recv_buf[64];
    memset(recv_buf, 0, sizeof(recv_buf));
    int received_fd = -1;
    int recv_ret = recv_fd_with_payload(sv[1], recv_buf, sizeof(recv_buf) - 1, 0, &received_fd);
    log_result("recvmsg SCM_RIGHTS", recv_ret == (int)strlen(msg1));
    log_result("payload matches", strcmp(recv_buf, msg1) == 0);
    log_result("received fd present", received_fd >= 0);

    if (received_fd >= 0) {
        char file_buf[64];
        memset(file_buf, 0, sizeof(file_buf));
        int read_ret = (int)read(received_fd, file_buf, sizeof(file_buf) - 1);
        log_result("read from received fd", read_ret == (int)(sizeof(file_payload) - 1));
        log_result("received fd content matches", strcmp(file_buf, file_payload) == 0);
    }

    const char msg2[] = "cloexec-check";
    send_ret = send_fd_with_payload(sv[0], data_fd, msg2);
    log_result("sendmsg second SCM_RIGHTS", send_ret == (int)strlen(msg2));

    memset(recv_buf, 0, sizeof(recv_buf));
    int cloexec_fd = -1;
    recv_ret = recv_fd_with_payload(sv[1], recv_buf, sizeof(recv_buf) - 1, MSG_CMSG_CLOEXEC, &cloexec_fd);
    log_result("recvmsg MSG_CMSG_CLOEXEC", recv_ret == (int)strlen(msg2));
    log_result("second payload matches", strcmp(recv_buf, msg2) == 0);
    log_result("second received fd present", cloexec_fd >= 0);

    if (cloexec_fd >= 0) {
        int fd_flags = fcntl(cloexec_fd, F_GETFD, 0);
        log_result("F_GETFD on received cloexec fd", fd_flags >= 0);
        if (fd_flags >= 0) {
            log_result("FD_CLOEXEC installed", (fd_flags & FD_CLOEXEC) != 0);
        }
    }

    close_if_valid(cloexec_fd);
    close_if_valid(received_fd);
    close_if_valid(sv[0]);
    close_if_valid(sv[1]);
    close_if_valid(data_fd);
    unlink("/scmtest.tmp");

    if (g_failures == 0) {
        out("scmtest: PASS\n");
        return 0;
    }

    out("scmtest: FAIL\n");
    return 1;
}

extern "C" int scmtest_main_cpp(int argc, char *argv[], char *envp[]) asm("_Z4mainiPPcS0_");
extern "C" int scmtest_main_cpp(int, char **, char **)
{
    return scmtest_main_impl();
}
