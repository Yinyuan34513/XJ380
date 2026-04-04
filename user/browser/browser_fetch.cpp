#include "browser_fetch.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

namespace
{

constexpr int HTTP_READ_BUFFER_SIZE = 4096;

struct ParsedUrl
{
    std::string scheme;
    std::string host;
    std::uint16_t port = 0;
    std::string path;
};

struct RawHttpResponse
{
    int         status_code = 0;
    std::string headers;
    std::string body;
    std::string content_type;
    std::string charset;
    std::string transfer_encoding;
};

struct ResponseAccumulator
{
    std::string data;
    bool        headers_parsed = false;
    bool        response_complete = false;
    std::size_t header_end = 0;
    std::size_t delimiter_size = 0;
    long long   content_length = -1;
    bool        chunked = false;
};

static bool parse_chunked_body(const std::string &input, std::string &output);

static std::string trim_copy(const std::string &value)
{
    std::size_t start = 0;
    while (start < value.size() && std::isspace(static_cast<unsigned char>(value[start])) != 0) {
        ++start;
    }

    std::size_t end = value.size();
    while (end > start && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }

    return value.substr(start, end - start);
}

static std::string lower_copy(const std::string &value)
{
    std::string lowered = value;
    for (char &ch : lowered) {
        ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    }
    return lowered;
}

static long long parse_positive_decimal(const std::string &text)
{
    long long value = 0;
    bool      have_digit = false;

    for (char ch : text) {
        if (ch < '0' || ch > '9') {
            break;
        }
        have_digit = true;
        value = value * 10 + static_cast<long long>(ch - '0');
    }

    return have_digit ? value : -1;
}

static bool parse_url(const std::string &input, ParsedUrl &parsed, std::string &error)
{
    std::string url = trim_copy(input);
    if (url.empty()) {
        error = "empty url";
        return false;
    }

    std::size_t scheme_sep = url.find("://");
    if (scheme_sep == std::string::npos) {
        url = "https://" + url;
        scheme_sep = url.find("://");
    }

    parsed.scheme = lower_copy(url.substr(0, scheme_sep));
    if (parsed.scheme != "http" && parsed.scheme != "https") {
        error = "unsupported scheme";
        return false;
    }

    std::size_t host_start = scheme_sep + 3;
    std::size_t path_start = url.find('/', host_start);
    std::string authority = path_start == std::string::npos ? url.substr(host_start)
                                                            : url.substr(host_start, path_start - host_start);
    parsed.path = path_start == std::string::npos ? "/" : url.substr(path_start);

    if (authority.empty()) {
        error = "missing host";
        return false;
    }

    std::size_t port_sep = authority.rfind(':');
    if (port_sep != std::string::npos) {
        parsed.host = authority.substr(0, port_sep);
        const std::string port_text = authority.substr(port_sep + 1);
        const long        port_value = std::strtol(port_text.c_str(), nullptr, 10);
        if (port_value <= 0 || port_value > 65535) {
            error = "bad port";
            return false;
        }
        parsed.port = static_cast<std::uint16_t>(port_value);
    } else {
        parsed.host = authority;
        parsed.port = parsed.scheme == "https" ? 443 : 80;
    }

    if (parsed.host.empty()) {
        error = "missing host";
        return false;
    }

    return true;
}

static int append_response_chunk(const char *data, int len, void *user_data)
{
    ResponseAccumulator *response = static_cast<ResponseAccumulator *>(user_data);
    if (response == nullptr || data == nullptr || len <= 0) {
        return -1;
    }

    response->data.append(data, static_cast<std::size_t>(len));

    if (!response->headers_parsed) {
        std::size_t header_end = response->data.find("\r\n\r\n");
        std::size_t delimiter_size = 4;
        if (header_end == std::string::npos) {
            header_end = response->data.find("\n\n");
            delimiter_size = 2;
        }

        if (header_end != std::string::npos) {
            response->headers_parsed = true;
            response->header_end = header_end;
            response->delimiter_size = delimiter_size;

            std::size_t cursor = 0;
            while (cursor < header_end) {
                std::size_t next = response->data.find('\n', cursor);
                if (next == std::string::npos || next > header_end) {
                    next = header_end;
                }
                std::string line = response->data.substr(cursor, next - cursor);
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }

                const std::size_t colon = line.find(':');
                if (colon != std::string::npos) {
                    const std::string key = lower_copy(trim_copy(line.substr(0, colon)));
                    const std::string value = trim_copy(line.substr(colon + 1));
                    if (key == "content-length") {
                        response->content_length = parse_positive_decimal(value);
                    } else if (key == "transfer-encoding") {
                        response->chunked = lower_copy(value).find("chunked") != std::string::npos;
                    }
                }

                if (next == header_end) {
                    break;
                }
                cursor = next + 1;
            }
        }
    }

    if (response->headers_parsed) {
        const std::size_t body_offset = response->header_end + response->delimiter_size;
        if (response->chunked) {
            std::string decoded;
            if (parse_chunked_body(response->data.substr(body_offset), decoded)) {
                response->response_complete = true;
            }
        } else if (response->content_length >= 0) {
            const std::size_t body_size = response->data.size() >= body_offset ? response->data.size() - body_offset : 0;
            if (body_size >= static_cast<std::size_t>(response->content_length)) {
                response->response_complete = true;
            }
        }
    }

    if (response->response_complete) {
        return 1;
    }
    return 0;
}

static int connect_tcp_socket(const ParsedUrl &url)
{
    addrinfo hints {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    char port_buffer[16];
    std::snprintf(port_buffer, sizeof(port_buffer), "%u", static_cast<unsigned int>(url.port));

    addrinfo *result = nullptr;
    if (getaddrinfo(url.host.c_str(), port_buffer, &hints, &result) != 0 || result == nullptr || result->ai_addr == nullptr) {
        if (result != nullptr) {
            freeaddrinfo(result);
        }
        return -1;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        freeaddrinfo(result);
        return -1;
    }

    const int original_flags = fcntl(sockfd, F_GETFL, 0);
    if (original_flags >= 0) {
        fcntl(sockfd, F_SETFL, static_cast<unsigned long long>(original_flags | O_NONBLOCK));
    }

    int connect_ret = connect(sockfd, result->ai_addr, result->ai_addrlen);
    if (connect_ret < 0 && connect_ret != -EINPROGRESS && connect_ret != -EALREADY && connect_ret != -EWOULDBLOCK) {
        close(sockfd);
        freeaddrinfo(result);
        return -1;
    }

    pollfd fd {};
    fd.fd = sockfd;
    fd.events = POLLOUT;
    if (poll(&fd, 1, 5000) <= 0 || (fd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
        close(sockfd);
        freeaddrinfo(result);
        return -1;
    }

    if (original_flags >= 0) {
        fcntl(sockfd, F_SETFL, static_cast<unsigned long long>(original_flags));
    }

    freeaddrinfo(result);
    return sockfd;
}

static bool send_all(int sockfd, const char *data, std::size_t len)
{
    std::size_t sent = 0;
    while (sent < len) {
        const int ret = write(sockfd, const_cast<char *>(data + sent), static_cast<unsigned long long>(len - sent));
        if (ret <= 0) {
            return false;
        }
        sent += static_cast<std::size_t>(ret);
    }
    return true;
}

static BrowserFetchResult fetch_http(const ParsedUrl &url)
{
    BrowserFetchResult result;
    result.final_url = url.scheme + "://" + url.host + url.path;

    const int sockfd = connect_tcp_socket(url);
    if (sockfd < 0) {
        result.error = "connect failed";
        return result;
    }

    std::string request;
    request += "GET ";
    request += url.path;
    request += " HTTP/1.1\r\nHost: ";
    request += url.host;
    request += "\r\nUser-Agent: XJ380Browser/0.1\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\nAccept-Encoding: identity\r\nConnection: close\r\n\r\n";

    if (!send_all(sockfd, request.c_str(), request.size())) {
        close(sockfd);
        result.error = "send failed";
        return result;
    }

    ResponseAccumulator response;
    char                buffer[HTTP_READ_BUFFER_SIZE];

    while (true) {
        pollfd fd {};
        fd.fd = sockfd;
        fd.events = POLLIN;
        const int wait_ret = poll(&fd, 1, 5000);
        if (wait_ret < 0) {
            close(sockfd);
            result.error = "poll failed";
            return result;
        }
        if (wait_ret == 0) {
            close(sockfd);
            result.error = "timeout";
            return result;
        }

        const int got = read(sockfd, buffer, sizeof(buffer));
        if (got < 0) {
            close(sockfd);
            result.error = "recv failed";
            return result;
        }
        if (got == 0) {
            break;
        }
        response.data.append(buffer, static_cast<std::size_t>(got));
    }

    close(sockfd);
    result.ok = true;
    result.body = std::move(response.data);
    return result;
}

static BrowserFetchResult fetch_https(const ParsedUrl &url)
{
    BrowserFetchResult result;
    result.final_url = url.scheme + "://" + url.host + url.path;

    xhttp_request_t request {};
    request.method = "GET";
    request.host = url.host.c_str();
    request.port = url.port;
    request.path = url.path.c_str();
    request.body = nullptr;
    request.content_type = "text/plain";
    request.extra_headers = "Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\nAccept-Encoding: identity";

    ResponseAccumulator response;
    const int status = xtls_request(&request, append_response_chunk, &response);
    if (status != 0) {
        result.error = "tls request failed";
        return result;
    }

    result.ok = true;
    result.body = std::move(response.data);
    return result;
}

static bool parse_chunked_body(const std::string &input, std::string &output)
{
    std::size_t cursor = 0;
    output.clear();

    while (cursor < input.size()) {
        std::size_t line_end = input.find("\r\n", cursor);
        std::size_t line_skip = 2;
        if (line_end == std::string::npos) {
            line_end = input.find('\n', cursor);
            line_skip = 1;
        }
        if (line_end == std::string::npos) {
            return false;
        }

        const std::string chunk_size_text = trim_copy(input.substr(cursor, line_end - cursor));
        const std::size_t semicolon = chunk_size_text.find(';');
        const std::string hex_size = semicolon == std::string::npos ? chunk_size_text
                                                                    : chunk_size_text.substr(0, semicolon);
        const unsigned long chunk_size = std::strtoul(hex_size.c_str(), nullptr, 16);
        cursor = line_end + line_skip;

        if (chunk_size == 0) {
            return true;
        }

        if (cursor + chunk_size > input.size()) {
            return false;
        }

        output.append(input, cursor, chunk_size);
        cursor += chunk_size;

        if (cursor + 2 <= input.size() && input.compare(cursor, 2, "\r\n") == 0) {
            cursor += 2;
        } else if (cursor < input.size() && input[cursor] == '\n') {
            cursor += 1;
        } else {
            return false;
        }
    }

    return false;
}

static bool split_http_response(const std::string &raw, RawHttpResponse &response)
{
    std::size_t header_end = raw.find("\r\n\r\n");
    std::size_t delimiter_size = 4;
    if (header_end == std::string::npos) {
        header_end = raw.find("\n\n");
        delimiter_size = 2;
    }
    if (header_end == std::string::npos) {
        return false;
    }

    response.headers = raw.substr(0, header_end);
    response.body = raw.substr(header_end + delimiter_size);

    std::size_t line_end = response.headers.find('\n');
    std::string status_line = line_end == std::string::npos ? response.headers
                                                            : response.headers.substr(0, line_end);
    if (!status_line.empty() && status_line.back() == '\r') {
        status_line.pop_back();
    }

    const std::size_t first_space = status_line.find(' ');
    if (first_space != std::string::npos) {
        response.status_code = std::atoi(status_line.c_str() + first_space + 1);
    }

    std::size_t cursor = line_end == std::string::npos ? response.headers.size() : line_end + 1;
    while (cursor < response.headers.size()) {
        std::size_t next = response.headers.find('\n', cursor);
        std::string line = next == std::string::npos ? response.headers.substr(cursor)
                                                     : response.headers.substr(cursor, next - cursor);
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        const std::size_t colon = line.find(':');
        if (colon != std::string::npos) {
            const std::string key = lower_copy(trim_copy(line.substr(0, colon)));
            const std::string value = trim_copy(line.substr(colon + 1));
            if (key == "content-type") {
                response.content_type = value;
                const std::string lowered = lower_copy(value);
                const std::size_t charset_pos = lowered.find("charset=");
                if (charset_pos != std::string::npos) {
                    response.charset = trim_copy(value.substr(charset_pos + 8));
                }
            } else if (key == "transfer-encoding") {
                response.transfer_encoding = lower_copy(value);
            }
        }

        if (next == std::string::npos) {
            break;
        }
        cursor = next + 1;
    }

    if (response.transfer_encoding.find("chunked") != std::string::npos) {
        std::string decoded;
        if (parse_chunked_body(response.body, decoded)) {
            response.body.swap(decoded);
        }
    }

    return true;
}

} // namespace

BrowserFetchResult browser_fetch_url(const std::string &input_url)
{
    BrowserFetchResult result;
    ParsedUrl          url;

    if (!parse_url(input_url, url, result.error)) {
        return result;
    }

    BrowserFetchResult raw_result = url.scheme == "https" ? fetch_https(url) : fetch_http(url);
    if (!raw_result.ok) {
        return raw_result;
    }

    RawHttpResponse response;
    if (!split_http_response(raw_result.body, response)) {
        raw_result.error = "bad http response";
        raw_result.ok = false;
        return raw_result;
    }

    result.ok = response.status_code >= 200 && response.status_code < 400;
    result.status_code = response.status_code;
    result.final_url = raw_result.final_url;
    result.content_type = response.content_type;
    result.charset = response.charset;
    result.body = response.body;
    if (!result.ok) {
        result.error = "http status " + std::to_string(response.status_code);
    }
    return result;
}
