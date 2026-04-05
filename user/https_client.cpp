#include "https_client.h"

#include <mbedtls/ctr_drbg.h>
#include <mbedtls/debug.h>
#include <mbedtls/ssl.h>
#include <mbedtls/ssl_ciphersuites.h>
#include <mbedtls/net_sockets.h>

extern "C" int xtls_prepare_runtime(uint64_t *saved_fs, void **tls_block);
extern "C" void xtls_restore_runtime(uint64_t saved_fs, void *tls_block);

typedef struct xtls_seed_state
{
    uint64_t state;
} xtls_seed_state_t;

static uint64_t xtls_rdtsc()
{
    uint32_t lo = 0;
    uint32_t hi = 0;
    __asm__ volatile("rdtsc" : "=a"(lo), "=d"(hi));
    return ((uint64_t)hi << 32) | lo;
}

static uint64_t xtls_mix64(uint64_t x)
{
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    return x * 2685821657736338717ULL;
}

static int xtls_entropy(void *ctx, unsigned char *output, size_t len)
{
    xtls_seed_state_t *seed = (xtls_seed_state_t *)ctx;
    if (seed == NULL || output == NULL) return -1;

    for (size_t i = 0; i < len; ++i) {
        seed->state ^= xtls_rdtsc() + ((uint64_t)(uintptr_t)&output[i] << 7);
        seed->state = xtls_mix64(seed->state + 0x9e3779b97f4a7c15ULL);
        output[i]   = (unsigned char)(seed->state >> ((i & 7U) * 8U));
    }

    return 0;
}

static int xtls_connect_socket(const xhttp_request_t *request)
{
    return xhttp_connect_socket_fd(request->host, request->port);
}

static int xtls_socket_send(void *ctx, const unsigned char *buf, size_t len)
{
    int fd = *(int *)ctx;
    int sent = write(fd, (char *)buf, (uint64_t)len);
    if (sent < 0) {
        if (sent == -EWOULDBLOCK || sent == -EAGAIN) {
            int wait_status = xhttp_wait_fd(fd, POLLOUT, XHTTP_IO_TIMEOUT_MS);
            if (wait_status == XHTTP_ERR_TIMEOUT) return MBEDTLS_ERR_SSL_TIMEOUT;
            if (wait_status != XHTTP_OK) {
                char logbuf[96];
                snprintf(logbuf, sizeof(logbuf), "xtls: send retry wait status=%d\n", wait_status);
                xapi_OutputSerial(logbuf);
                return MBEDTLS_ERR_NET_POLL_FAILED;
            }
            sent = write(fd, (char *)buf, (uint64_t)len);
        }

        if (sent < 0) {
            char logbuf[96];
            snprintf(logbuf, sizeof(logbuf), "xtls: send write ret=%d\n", sent);
            xapi_OutputSerial(logbuf);
            if (sent == -EWOULDBLOCK || sent == -EAGAIN) return MBEDTLS_ERR_SSL_WANT_WRITE;
            return MBEDTLS_ERR_NET_SEND_FAILED;
        }
    }
    {
        char logbuf[96];
        snprintf(logbuf, sizeof(logbuf), "xtls: send wrote=%d len=%u\n", sent, (unsigned int)len);
        xapi_OutputSerial(logbuf);
    }
    return sent;
}

static int xtls_socket_recv(void *ctx, unsigned char *buf, size_t len)
{
    int fd = *(int *)ctx;
    int wait_status = xhttp_wait_fd(fd, POLLIN, XHTTP_IO_TIMEOUT_MS);
    if (wait_status == XHTTP_ERR_TIMEOUT) return MBEDTLS_ERR_SSL_TIMEOUT;
    if (wait_status != XHTTP_OK) {
        char logbuf[96];
        snprintf(logbuf, sizeof(logbuf), "xtls: recv wait status=%d\n", wait_status);
        xapi_OutputSerial(logbuf);
        return MBEDTLS_ERR_NET_POLL_FAILED;
    }

    int got = read(fd, (char *)buf, (uint64_t)len);
    if (got < 0) {
        char logbuf[96];
        snprintf(logbuf, sizeof(logbuf), "xtls: recv read ret=%d\n", got);
        xapi_OutputSerial(logbuf);
        if (got == -EWOULDBLOCK || got == -EAGAIN) return MBEDTLS_ERR_SSL_WANT_READ;
        return MBEDTLS_ERR_NET_RECV_FAILED;
    }
    {
        char logbuf[96];
        snprintf(logbuf, sizeof(logbuf), "xtls: recv got=%d\n", got);
        xapi_OutputSerial(logbuf);
    }
    return got;
}

static void xtls_log_mbedtls_error(const char *stage, int ret)
{
    char logbuf[128];
    snprintf(logbuf, sizeof(logbuf), "xtls: %s ret=%d hex=0x%x\n", stage, ret, (unsigned int)(-ret));
    xapi_OutputSerial(logbuf);
}

static int xtls_headers_need_crlf(const char *headers)
{
    size_t len = headers ? strlen(headers) : 0;
    if (len == 0) {
        return 0;
    }
    if (len >= 2 && headers[len - 2] == '\r' && headers[len - 1] == '\n') {
        return 0;
    }
    return 1;
}

static int xtls_ssl_write_all(mbedtls_ssl_context *ssl, const char *data, int len)
{
    int sent_total = 0;

    while (sent_total < len) {
        int ret = mbedtls_ssl_write(ssl, (const unsigned char *)data + sent_total, (size_t)(len - sent_total));
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
        if (ret == MBEDTLS_ERR_SSL_TIMEOUT) return XHTTP_ERR_TIMEOUT;
        if (ret <= 0) {
            xtls_log_mbedtls_error("ssl_write", ret);
            return XHTTP_ERR_TLS_SEND;
        }
        sent_total += ret;
    }

    return XHTTP_OK;
}

static int xtls_ssl_recv_loop(mbedtls_ssl_context *ssl,
                              xhttp_response_callback_t callback,
                              void *user_data)
{
    char buffer[XHTTP_RECV_BUFFER_SIZE];

    while (true) {
        int ret = mbedtls_ssl_read(ssl, (unsigned char *)buffer, sizeof(buffer));
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
        if (ret == MBEDTLS_ERR_SSL_TIMEOUT) return XHTTP_ERR_TIMEOUT;
        if (ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY || ret == 0) break;
        if (ret < 0) {
            xtls_log_mbedtls_error("ssl_read", ret);
            return XHTTP_ERR_TLS_RECV;
        }

        if (callback != NULL && callback(buffer, ret, user_data) != 0) break;
    }

    return XHTTP_OK;
}

static int xtls_request_once(const xhttp_request_t *request,
                             xhttp_response_callback_t callback,
                             void *user_data,
                             mbedtls_ssl_protocol_version max_tls_version)
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

    int sockfd = xtls_connect_socket(request);
    if (sockfd < 0) return sockfd;

    uint64_t tls_saved_fs = 0;
    void    *tls_block = NULL;
    if (xtls_prepare_runtime(&tls_saved_fs, &tls_block) != 0) {
        xapi_OutputSerial((char *)"xtls: prepare runtime failed\n");
        shutdown(sockfd, SHUT_RDWR);
        close(sockfd);
        return XHTTP_ERR_TLS_INIT;
    }

    xtls_seed_state_t seed_state;
    seed_state.state = xtls_rdtsc() ^ (uint64_t)(uintptr_t)request ^ (uint64_t)(uintptr_t)request->host;

    mbedtls_ssl_context      ssl;
    mbedtls_ssl_config       conf;
    mbedtls_ctr_drbg_context drbg;
    const char              *path = NULL;
    const char              *body = NULL;
    const char              *content_type = NULL;
    const char              *extra_headers = NULL;
    const char              *extra_headers_suffix = "";
    int                      body_len = 0;
    char                     request_text[XHTTP_REQUEST_BUFFER_SIZE];
    char                     host_header[320];
    int                      request_len = 0;

    mbedtls_ssl_init(&ssl);
    mbedtls_ssl_config_init(&conf);
    mbedtls_ctr_drbg_init(&drbg);

    static const char personal[] = "xj380-httpget-tls";

    int status = XHTTP_OK;
    int ret = mbedtls_ctr_drbg_seed(&drbg, xtls_entropy, &seed_state,
                                    (const unsigned char *)personal, sizeof(personal) - 1);
    if (ret != 0) {
        char logbuf[96];
        snprintf(logbuf, sizeof(logbuf), "xtls: ctr_drbg_seed ret=%d\n", ret);
        xapi_OutputSerial(logbuf);
        status = XHTTP_ERR_TLS_SEED;
        goto cleanup;
    }

    ret = mbedtls_ssl_config_defaults(&conf,
                                      MBEDTLS_SSL_IS_CLIENT,
                                      MBEDTLS_SSL_TRANSPORT_STREAM,
                                      MBEDTLS_SSL_PRESET_DEFAULT);
    if (ret != 0) {
        char logbuf[96];
        snprintf(logbuf, sizeof(logbuf), "xtls: ssl_config_defaults ret=%d\n", ret);
        xapi_OutputSerial(logbuf);
        status = XHTTP_ERR_TLS_CONFIG;
        goto cleanup;
    }

    mbedtls_ssl_conf_min_tls_version(&conf, MBEDTLS_SSL_VERSION_TLS1_2);
    mbedtls_ssl_conf_max_tls_version(&conf, max_tls_version);
    mbedtls_ssl_conf_session_tickets(&conf, MBEDTLS_SSL_SESSION_TICKETS_DISABLED);

    mbedtls_ssl_conf_authmode(&conf, MBEDTLS_SSL_VERIFY_NONE);
    mbedtls_ssl_conf_rng(&conf, mbedtls_ctr_drbg_random, &drbg);
    mbedtls_debug_set_threshold(0);

    ret = mbedtls_ssl_setup(&ssl, &conf);
    if (ret != 0) {
        char logbuf[96];
        snprintf(logbuf, sizeof(logbuf), "xtls: ssl_setup ret=%d\n", ret);
        xapi_OutputSerial(logbuf);
        status = XHTTP_ERR_TLS_INIT;
        goto cleanup;
    }

    ret = mbedtls_ssl_set_hostname(&ssl, request->host);
    if (ret != 0) {
        char logbuf[96];
        snprintf(logbuf, sizeof(logbuf), "xtls: set_hostname ret=%d\n", ret);
        xapi_OutputSerial(logbuf);
        status = XHTTP_ERR_TLS_INIT;
        goto cleanup;
    }
    mbedtls_ssl_set_bio(&ssl, &sockfd, xtls_socket_send, xtls_socket_recv, NULL);

    while ((ret = mbedtls_ssl_handshake(&ssl)) != 0) {
        if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) continue;
        xtls_log_mbedtls_error("handshake", ret);
        if (ret == MBEDTLS_ERR_SSL_TIMEOUT) {
            status = XHTTP_ERR_TIMEOUT;
        } else {
            status = XHTTP_ERR_TLS_HANDSHAKE;
        }
        goto cleanup;
    }
    {
        char logbuf[160];
        snprintf(logbuf, sizeof(logbuf), "xtls: handshake ok version=%s cipher=%s\n",
                 mbedtls_ssl_get_version(&ssl),
                 mbedtls_ssl_get_ciphersuite(&ssl));
        xapi_OutputSerial(logbuf);
    }

    path          = request->path[0] ? request->path : "/";
    body          = request->body;
    content_type  = request->content_type ? request->content_type : "text/plain";
    extra_headers = request->extra_headers ? request->extra_headers : "";
    body_len      = body ? (int)strlen(body) : 0;
    extra_headers_suffix = xtls_headers_need_crlf(extra_headers) ? "\r\n" : "";
    xhttp_format_host_header(request->host, host_header, sizeof(host_header));

    if (body_len > 0) {
        request_len = snprintf(request_text, sizeof(request_text),
                               "%s %s HTTP/1.1\r\n"
                               "Host: %s\r\n"
                               "User-Agent: XJ380-httpget/1.0\r\n"
                               "Accept: */*\r\n"
                               "Connection: close\r\n"
                               "Content-Type: %s\r\n"
                               "Content-Length: %d\r\n"
                               "%s"
                               "%s"
                               "\r\n"
                               "%s",
                               request->method, path, host_header, content_type, body_len,
                               extra_headers, extra_headers_suffix, body);
    } else {
        request_len = snprintf(request_text, sizeof(request_text),
                               "%s %s HTTP/1.1\r\n"
                               "Host: %s\r\n"
                               "User-Agent: XJ380-httpget/1.0\r\n"
                               "Accept: */*\r\n"
                               "Connection: close\r\n"
                               "%s"
                               "%s"
                               "\r\n",
                               request->method, path, host_header, extra_headers, extra_headers_suffix);
    }

    if (request_len <= 0 || request_len >= (int)sizeof(request_text)) {
        status = XHTTP_ERR_REQUEST_BUILD;
        goto cleanup;
    }

    status = xtls_ssl_write_all(&ssl, request_text, request_len);
    if (status == XHTTP_OK) {
        status = xtls_ssl_recv_loop(&ssl, callback, user_data);
    }

cleanup:
    if (status != XHTTP_OK) {
        char logbuf[96];
        snprintf(logbuf, sizeof(logbuf), "xtls: request status=%d maxver=0x%x\n", status, max_tls_version);
        xapi_OutputSerial(logbuf);
    }
    mbedtls_ssl_free(&ssl);
    mbedtls_ssl_config_free(&conf);
    mbedtls_ctr_drbg_free(&drbg);
    xtls_restore_runtime(tls_saved_fs, tls_block);
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
    return status;
}

int xtls_request(const xhttp_request_t *request,
                 xhttp_response_callback_t callback,
                 void *user_data)
{
    int status = xtls_request_once(request, callback, user_data, MBEDTLS_SSL_VERSION_TLS1_3);
    if (status == XHTTP_OK || request == NULL || request->method == NULL) {
        return status;
    }

    if ((strcmp(request->method, "GET") != 0 && strcmp(request->method, "HEAD") != 0) ||
        (status != XHTTP_ERR_TLS_HANDSHAKE && status != XHTTP_ERR_TLS_SEND &&
         status != XHTTP_ERR_TLS_RECV && status != XHTTP_ERR_TIMEOUT)) {
        return status;
    }

    xapi_OutputSerial((char *)"xtls: retry with tls1.2\n");
    return xtls_request_once(request, callback, user_data, MBEDTLS_SSL_VERSION_TLS1_2);
}
