#include <arpa/inet.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

static volatile sig_atomic_t g_stop = 0;

static void on_signal(int signo)
{
    (void)signo;
    g_stop = 1;
}

static int connect_guest(const char *host, uint16_t port)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port   = htons(port);
    if (inet_pton(AF_INET, host, &addr.sin_addr) != 1) {
        fprintf(stderr, "invalid host: %s\n", host);
        close(fd);
        return -1;
    }

    if (connect(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("connect");
        close(fd);
        return -1;
    }

    return fd;
}

int main(int argc, char **argv)
{
    const char *host = "127.0.0.1";
    uint16_t    port = 2323;

    if (argc >= 2) {
        host = argv[1];
    }
    if (argc >= 3) {
        long parsed = strtol(argv[2], NULL, 10);
        if (parsed <= 0 || parsed > 65535) {
            fprintf(stderr, "invalid port: %s\n", argv[2]);
            return 1;
        }
        port = (uint16_t)parsed;
    }

    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);

    int sockfd = connect_guest(host, port);
    if (sockfd < 0) {
        return 1;
    }

    printf("Connected to XJ380 chat at %s:%u\n", host, port);
    printf("Type and press Enter to send. Ctrl+C to quit.\n");
    fflush(stdout);

    struct pollfd fds[2];
    fds[0].fd      = STDIN_FILENO;
    fds[0].events  = POLLIN;
    fds[0].revents = 0;
    fds[1].fd      = sockfd;
    fds[1].events  = POLLIN;
    fds[1].revents = 0;

    char buffer[512];

    while (!g_stop) {
        int ready = poll(fds, 2, -1);
        if (ready < 0) {
            if (errno == EINTR) {
                continue;
            }
            perror("poll");
            break;
        }

        if (fds[1].revents & (POLLHUP | POLLERR | POLLNVAL)) {
            printf("Guest disconnected.\n");
            break;
        }

        if (fds[1].revents & POLLIN) {
            ssize_t got = read(sockfd, buffer, sizeof(buffer));
            if (got <= 0) {
                printf("Guest disconnected.\n");
                break;
            }
            printf("[xj380] %.*s", (int)got, buffer);
            if (buffer[got - 1] != '\n') {
                putchar('\n');
            }
            fflush(stdout);
        }

        if (fds[0].revents & POLLIN) {
            ssize_t got = read(STDIN_FILENO, buffer, sizeof(buffer));
            if (got > 0) {
                ssize_t sent = send(sockfd, buffer, (size_t)got, 0);
                if (sent < 0) {
                    perror("send");
                    break;
                }
            } else if (got == 0) {
                break;
            }
        }
    }

    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
    return 0;
}
