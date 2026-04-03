#pragma once

#include "x3api.h"
#include "xposix/errno.h"

#ifndef XHTTP_REQUEST_BUFFER_SIZE
#define XHTTP_REQUEST_BUFFER_SIZE 2048
#endif

#ifndef XHTTP_RECV_BUFFER_SIZE
#define XHTTP_RECV_BUFFER_SIZE 512
#endif

typedef int (*xhttp_response_callback_t)(const char *data, int len, void *user_data);

typedef struct xhttp_request {
    const char *method;
    const char *host;
    uint16_t    port;
    const char *path;
    const char *body;
    const char *content_type;
    const char *extra_headers;
} xhttp_request_t;

enum {
    XHTTP_OK                = 0,
    XHTTP_ERR_BAD_ARGUMENT  = -1,
    XHTTP_ERR_BAD_METHOD    = -2,
    XHTTP_ERR_BAD_HOST      = -3,
    XHTTP_ERR_BAD_PORT      = -4,
    XHTTP_ERR_SOCKET        = -5,
    XHTTP_ERR_CONNECT       = -6,
    XHTTP_ERR_REQUEST_BUILD = -7,
    XHTTP_ERR_SEND          = -8,
    XHTTP_ERR_RECV          = -9,
    XHTTP_ERR_TIMEOUT       = -10
};

static inline int xhttp_is_method_char(char ch)
{
    return (ch >= 'A' && ch <= 'Z') ||
           (ch >= 'a' && ch <= 'z') ||
           (ch >= '0' && ch <= '9') ||
           ch == '!' || ch == '#' || ch == '$' || ch == '%' || ch == '&' || ch == '\'' ||
           ch == '*' || ch == '+' || ch == '-' || ch == '.' || ch == '^' || ch == '_' ||
           ch == '`' || ch == '|' || ch == '~';
}

static inline int xhttp_send_all(int sockfd, const char *data, int len)
{
    int sent_total = 0;

    while (sent_total < len) {
        int sent = write(sockfd, (char *)data + sent_total, (uint64_t)(len - sent_total));
        if (sent <= 0) {
            return XHTTP_ERR_SEND;
        }
        sent_total += sent;
    }

    return XHTTP_OK;
}

static inline int xhttp_recv_loop(int sockfd, xhttp_response_callback_t callback, void *user_data)
{
    char buffer[XHTTP_RECV_BUFFER_SIZE];

    while (1) {
        int got = read(sockfd, buffer, sizeof(buffer));
        if (got < 0) {
            return XHTTP_ERR_RECV;
        }
        if (got == 0) {
            break;
        }
        if (callback != NULL && callback(buffer, got, user_data) != 0) {
            break;
        }
    }

    return XHTTP_OK;
}

static inline int xhttp_validate_method(const char *method)
{
    if (method == NULL || method[0] == '\0') {
        return 0;
    }

    for (const char *p = method; *p != '\0'; ++p) {
        if (!xhttp_is_method_char(*p)) {
            return 0;
        }
    }

    return 1;
}

static inline int xhttp_parse_ipv4(const char *text, uint32_t *out_addr)
{
    uint32_t parts[4] = {0, 0, 0, 0};
    int      part     = 0;
    uint32_t value    = 0;

    if (text == NULL || out_addr == NULL) {
        return 0;
    }

    for (const char *p = text;; ++p) {
        char ch = *p;
        if (isdigit(ch)) {
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

static inline int xhttp_request(const xhttp_request_t *request,
                                xhttp_response_callback_t callback,
                                void *user_data)
{
    if (request == NULL || request->host == NULL || request->path == NULL) {
        return XHTTP_ERR_BAD_ARGUMENT;
    }
    if (!xhttp_validate_method(request->method)) {
        return XHTTP_ERR_BAD_METHOD;
    }
    if (request->port == 0) {
        return XHTTP_ERR_BAD_PORT;
    }

    uint32_t addr = 0;
    if (!xhttp_parse_ipv4(request->host, &addr)) {
        return XHTTP_ERR_BAD_HOST;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        return XHTTP_ERR_SOCKET;
    }

    struct sockaddr_in addr_in;
    memset(&addr_in, 0, sizeof(addr_in));
    addr_in.sin_family      = AF_INET;
    addr_in.sin_port        = htons(request->port);
    addr_in.sin_addr.s_addr = addr;

    int connect_ret = connect(sockfd, (const struct sockaddr *)&addr_in, sizeof(addr_in));
    if (connect_ret < 0) {
        close(sockfd);
        return XHTTP_ERR_CONNECT;
    }

    const char *path          = request->path[0] ? request->path : "/";
    const char *body          = request->body;
    const char *content_type  = request->content_type ? request->content_type : "text/plain";
    const char *extra_headers = request->extra_headers ? request->extra_headers : "";
    int         body_len      = body ? (int)strlen(body) : 0;

    char request_text[XHTTP_REQUEST_BUFFER_SIZE];
    int  request_len = 0;

    if (body_len > 0) {
        request_len = snprintf(request_text, sizeof(request_text),
                               "%s %s HTTP/1.0\r\n"
                               "Host: %s\r\n"
                               "Connection: close\r\n"
                               "Content-Type: %s\r\n"
                               "Content-Length: %d\r\n"
                               "%s"
                               "\r\n"
                               "%s",
                               request->method, path, request->host, content_type, body_len, extra_headers, body);
    } else {
        request_len = snprintf(request_text, sizeof(request_text),
                               "%s %s HTTP/1.0\r\n"
                               "Host: %s\r\n"
                               "Connection: close\r\n"
                               "%s"
                               "\r\n",
                               request->method, path, request->host, extra_headers);
    }

    if (request_len <= 0 || request_len >= (int)sizeof(request_text)) {
        shutdown(sockfd, SHUT_RDWR);
        close(sockfd);
        return XHTTP_ERR_REQUEST_BUILD;
    }

    int status = xhttp_send_all(sockfd, request_text, request_len);
    if (status == XHTTP_OK) {
        status = xhttp_recv_loop(sockfd, callback, user_data);
    }

    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
    return status;
}

static inline int xhttp_request_method(const char *method,
                                       const char *host,
                                       uint16_t port,
                                       const char *path,
                                       const char *body,
                                       const char *content_type,
                                       const char *extra_headers,
                                       xhttp_response_callback_t callback,
                                       void *user_data)
{
    xhttp_request_t request;
    request.method        = method;
    request.host          = host;
    request.port          = port;
    request.path          = path;
    request.body          = body;
    request.content_type  = content_type;
    request.extra_headers = extra_headers;
    return xhttp_request(&request, callback, user_data);
}

static inline int xhttp_get_request(const char *host,
                                    uint16_t port,
                                    const char *path,
                                    const char *extra_headers,
                                    xhttp_response_callback_t callback,
                                    void *user_data)
{
    return xhttp_request_method("GET", host, port, path, NULL, NULL, extra_headers, callback, user_data);
}

static inline int xhttp_post_request(const char *host,
                                     uint16_t port,
                                     const char *path,
                                     const char *body,
                                     const char *content_type,
                                     const char *extra_headers,
                                     xhttp_response_callback_t callback,
                                     void *user_data)
{
    return xhttp_request_method("POST", host, port, path, body, content_type, extra_headers, callback, user_data);
}

static inline int xhttp_put_request(const char *host,
                                    uint16_t port,
                                    const char *path,
                                    const char *body,
                                    const char *content_type,
                                    const char *extra_headers,
                                    xhttp_response_callback_t callback,
                                    void *user_data)
{
    return xhttp_request_method("PUT", host, port, path, body, content_type, extra_headers, callback, user_data);
}

static inline int xhttp_patch_request(const char *host,
                                      uint16_t port,
                                      const char *path,
                                      const char *body,
                                      const char *content_type,
                                      const char *extra_headers,
                                      xhttp_response_callback_t callback,
                                      void *user_data)
{
    return xhttp_request_method("PATCH", host, port, path, body, content_type, extra_headers, callback, user_data);
}

static inline int xhttp_delete_request(const char *host,
                                       uint16_t port,
                                       const char *path,
                                       const char *body,
                                       const char *content_type,
                                       const char *extra_headers,
                                       xhttp_response_callback_t callback,
                                       void *user_data)
{
    return xhttp_request_method("DELETE", host, port, path, body, content_type, extra_headers, callback, user_data);
}

static inline int xhttp_head_request(const char *host,
                                     uint16_t port,
                                     const char *path,
                                     const char *extra_headers,
                                     xhttp_response_callback_t callback,
                                     void *user_data)
{
    return xhttp_request_method("HEAD", host, port, path, NULL, NULL, extra_headers, callback, user_data);
}

static inline int xhttp_options_request(const char *host,
                                        uint16_t port,
                                        const char *path,
                                        const char *extra_headers,
                                        xhttp_response_callback_t callback,
                                        void *user_data)
{
    return xhttp_request_method("OPTIONS", host, port, path, NULL, NULL, extra_headers, callback, user_data);
}

static inline int xhttp_trace_request(const char *host,
                                      uint16_t port,
                                      const char *path,
                                      const char *extra_headers,
                                      xhttp_response_callback_t callback,
                                      void *user_data)
{
    return xhttp_request_method("TRACE", host, port, path, NULL, NULL, extra_headers, callback, user_data);
}

static inline int xhttp_connect_request(const char *host,
                                        uint16_t port,
                                        const char *path,
                                        const char *extra_headers,
                                        xhttp_response_callback_t callback,
                                        void *user_data)
{
    return xhttp_request_method("CONNECT", host, port, path, NULL, NULL, extra_headers, callback, user_data);
}
