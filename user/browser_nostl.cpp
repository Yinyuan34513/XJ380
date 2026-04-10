#include "./xapi/include/x3api.h"
#include "browser_css_nostl.h"
#include "include/httpclient.h"
#include "include/https_client.h"

#define BROWSER_WINDOW_WIDTH       980
#define BROWSER_WINDOW_HEIGHT      700
#define BROWSER_LOOP_SLEEP_MS      16
#define BROWSER_PADDING            12
#define BROWSER_HEADER_HEIGHT      78
#define BROWSER_URL_MAX            768
#define BROWSER_STATUS_MAX         128
#define BROWSER_TITLE_MAX          160
#define BROWSER_HOST_MAX           260
#define BROWSER_PATH_MAX           1024
#define BROWSER_LINE_TEXT_MAX      192
#define BROWSER_MAX_LINES          4096
#define BROWSER_SCROLL_STEP        3
#define BROWSER_MAX_REDIRECTS      4
#define BROWSER_RAW_LIMIT          (1024 * 1024)
#define BROWSER_TEXT_LIMIT         (512 * 1024)

#define BROWSER_COLOR_BG           0xe8edf2ff
#define BROWSER_COLOR_HEADER       0x18324aff
#define BROWSER_COLOR_PANEL        0xfcfcfbff
#define BROWSER_COLOR_INPUT        0xffffffff
#define BROWSER_COLOR_INPUT_BORDER 0xb9c6d3ff
#define BROWSER_COLOR_TEXT         0x1f2933ff
#define BROWSER_COLOR_MUTED        0x6c7a89ff
#define BROWSER_COLOR_STATUS       0xc7f9ccff
#define BROWSER_COLOR_ERROR        0xc1121fff
#define BROWSER_COLOR_BUTTON       0x2a6f97ff
#define BROWSER_COLOR_BUTTON_TEXT  0xffffffff

typedef struct browser_Buffer
{
    char *data;
    int   length;
    int   capacity;
    bool  truncated;
} browser_Buffer;

typedef struct browser_ParsedUrl
{
    bool     use_tls;
    char     scheme[8];
    char     host[BROWSER_HOST_MAX];
    uint16_t port;
    char     path[BROWSER_PATH_MAX];
} browser_ParsedUrl;

typedef struct browser_ResponseMeta
{
    int   status_code;
    bool  chunked;
    bool  html_like;
    char  content_type[96];
    char  charset[32];
    char  location[BROWSER_URL_MAX];
    char *body;
    int   body_len;
} browser_ResponseMeta;

static HDLE           g_window = 0;
static bool           g_need_redraw = true;
static bool           g_fetch_requested = false;
static bool           g_need_exit = false;
static bool           g_input_focus = true;
static char           g_url_input[BROWSER_URL_MAX] = "https://example.com/";
static int            g_url_input_len = 20;
static char           g_status[BROWSER_STATUS_MAX] = "Ready";
static char           g_page_title[BROWSER_TITLE_MAX] = "Browser";
static char           g_current_url[BROWSER_URL_MAX] = "";
static char           g_lines[BROWSER_MAX_LINES][BROWSER_LINE_TEXT_MAX];
static int            g_line_count = 0;
static int            g_scroll_top = 0;
static int            g_scroll_pixels = 0;
static int            g_rich_content_height = 0;
static bool           g_rich_document = false;
static bool           g_mouse_in_content = false;
static browser_Buffer g_response_buffer = {0};
static browser_Buffer g_chunked_buffer = {0};
static browser_Buffer g_text_buffer = {0};
static browser_css_document *g_css_document = NULL;

static int browser_min(int a, int b)
{
    return a < b ? a : b;
}

static int browser_is_space(char ch)
{
    return ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r' || ch == '\f' || ch == '\v';
}

static int browser_is_printable(char ch)
{
    return ch >= ' ' && ch <= '~';
}

static char browser_to_lower(char ch)
{
    if (ch >= 'A' && ch <= 'Z') {
        return (char)(ch - 'A' + 'a');
    }
    return ch;
}

static int browser_stricmp(const char *lhs, const char *rhs)
{
    if (lhs == NULL || rhs == NULL) {
        return lhs == rhs ? 0 : (lhs == NULL ? -1 : 1);
    }

    while (*lhs != '\0' && *rhs != '\0') {
        char l = browser_to_lower(*lhs);
        char r = browser_to_lower(*rhs);
        if (l != r) {
            return (int)((unsigned char)l) - (int)((unsigned char)r);
        }
        ++lhs;
        ++rhs;
    }
    return (int)((unsigned char)browser_to_lower(*lhs)) -
           (int)((unsigned char)browser_to_lower(*rhs));
}

static bool browser_starts_with_nocase(const char *text, const char *prefix)
{
    if (text == NULL || prefix == NULL) {
        return false;
    }

    while (*prefix != '\0') {
        if (*text == '\0' || browser_to_lower(*text) != browser_to_lower(*prefix)) {
            return false;
        }
        ++text;
        ++prefix;
    }
    return true;
}

static void browser_strlcpy(char *dst, int dst_size, const char *src)
{
    if (dst == NULL || dst_size <= 0) {
        return;
    }

    if (src == NULL) {
        dst[0] = '\0';
        return;
    }

    int i = 0;
    while (i < dst_size - 1 && src[i] != '\0') {
        dst[i] = src[i];
        ++i;
    }
    dst[i] = '\0';
}

static void browser_trim_inplace(char *text)
{
    if (text == NULL) {
        return;
    }

    int len = (int)strlen(text);
    int start = 0;
    while (start < len && browser_is_space(text[start])) {
        ++start;
    }

    int end = len;
    while (end > start && browser_is_space(text[end - 1])) {
        --end;
    }

    if (start > 0) {
        int out_len = end - start;
        for (int i = 0; i < out_len; ++i) {
            text[i] = text[start + i];
        }
        text[out_len] = '\0';
        return;
    }
    text[end] = '\0';
}

static int browser_find_nocase(const char *text, const char *needle, int start)
{
    if (text == NULL || needle == NULL) {
        return -1;
    }

    int text_len = (int)strlen(text);
    int needle_len = (int)strlen(needle);
    if (needle_len <= 0 || start < 0 || start >= text_len) {
        return -1;
    }

    for (int i = start; i + needle_len <= text_len; ++i) {
        int matched = 1;
        for (int j = 0; j < needle_len; ++j) {
            if (browser_to_lower(text[i + j]) != browser_to_lower(needle[j])) {
                matched = 0;
                break;
            }
        }
        if (matched) {
            return i;
        }
    }
    return -1;
}

static bool browser_buffer_reserve(browser_Buffer *buffer, int need, int limit)
{
    if (buffer == NULL) {
        return false;
    }
    if (need < 0 || need > limit) {
        return false;
    }
    if (buffer->capacity >= need + 1) {
        return true;
    }

    int new_capacity = buffer->capacity > 0 ? buffer->capacity : 4096;
    while (new_capacity < need + 1) {
        if (new_capacity >= limit / 2) {
            new_capacity = limit + 1;
            break;
        }
        new_capacity *= 2;
    }
    if (new_capacity > limit + 1) {
        new_capacity = limit + 1;
    }

    char *new_data = (char *)realloc(buffer->data, (size_t)new_capacity);
    if (new_data == NULL) {
        return false;
    }

    buffer->data = new_data;
    buffer->capacity = new_capacity;
    if (buffer->length < 0) {
        buffer->length = 0;
    }
    buffer->data[buffer->length] = '\0';
    return true;
}

static void browser_buffer_reset(browser_Buffer *buffer)
{
    if (buffer == NULL) {
        return;
    }

    buffer->length = 0;
    buffer->truncated = false;
    if (buffer->data != NULL && buffer->capacity > 0) {
        buffer->data[0] = '\0';
    }
}

static bool browser_buffer_append(browser_Buffer *buffer, const char *data, int len, int limit)
{
    if (buffer == NULL || data == NULL || len <= 0) {
        return true;
    }

    if (buffer->length >= limit) {
        buffer->truncated = true;
        return true;
    }

    int writable = len;
    if (buffer->length + writable > limit) {
        writable = limit - buffer->length;
        buffer->truncated = true;
    }

    if (!browser_buffer_reserve(buffer, buffer->length + writable, limit)) {
        return false;
    }

    memcpy(buffer->data + buffer->length, data, (size_t)writable);
    buffer->length += writable;
    buffer->data[buffer->length] = '\0';
    return true;
}

static bool browser_buffer_append_char(browser_Buffer *buffer, char ch, int limit)
{
    return browser_buffer_append(buffer, &ch, 1, limit);
}

static int browser_response_append(const char *data, int len, void *user_data)
{
    browser_Buffer *buffer = (browser_Buffer *)user_data;
    return browser_buffer_append(buffer, data, len, BROWSER_RAW_LIMIT) ? 0 : -1;
}

static void browser_clear_lines()
{
    g_line_count = 0;
    g_scroll_top = 0;
}

static int browser_content_x1()
{
    return BROWSER_PADDING + 12;
}

static int browser_content_y1()
{
    return BROWSER_HEADER_HEIGHT + 32;
}

static int browser_content_x2()
{
    return BROWSER_WINDOW_WIDTH - BROWSER_PADDING - 12;
}

static int browser_content_y2()
{
    return BROWSER_WINDOW_HEIGHT - BROWSER_PADDING - 10;
}

static int browser_content_width()
{
    return browser_content_x2() - browser_content_x1();
}

static int browser_content_height()
{
    return browser_content_y2() - browser_content_y1();
}

static void browser_reset_rich_document()
{
    if (g_css_document != NULL) {
        browser_css_clear(g_css_document);
    }
    g_rich_document = false;
    g_rich_content_height = 0;
    g_scroll_pixels = 0;
    g_mouse_in_content = false;
}

static void browser_append_line_direct(const char *text)
{
    if (g_line_count >= BROWSER_MAX_LINES) {
        return;
    }

    browser_strlcpy(g_lines[g_line_count], BROWSER_LINE_TEXT_MAX, text == NULL ? "" : text);
    g_line_count++;
}

static void browser_append_blank_line()
{
    if (g_line_count > 0 && g_lines[g_line_count - 1][0] == '\0') {
        return;
    }
    browser_append_line_direct("");
}

static int browser_visible_lines()
{
    int content_h = browser_content_height();
    if (content_h <= 0) {
        return 0;
    }
    return content_h / 16;
}

static int browser_max_scroll()
{
    if (g_rich_document) {
        int max_scroll = g_rich_content_height - browser_content_height();
        return max_scroll > 0 ? max_scroll : 0;
    }

    int visible = browser_visible_lines();
    if (g_line_count <= visible) {
        return 0;
    }
    return g_line_count - visible;
}

static void browser_clamp_scroll()
{
    int max_scroll = browser_max_scroll();
    if (g_rich_document) {
        if (g_scroll_pixels < 0) {
            g_scroll_pixels = 0;
        }
        if (g_scroll_pixels > max_scroll) {
            g_scroll_pixels = max_scroll;
        }
    } else {
        if (g_scroll_top < 0) {
            g_scroll_top = 0;
        }
        if (g_scroll_top > max_scroll) {
            g_scroll_top = max_scroll;
        }
    }
}

static void browser_scroll_by(int delta)
{
    if (g_rich_document) {
        g_scroll_pixels += delta * 16;
    } else {
        g_scroll_top += delta;
    }
    browser_clamp_scroll();
}

static void browser_set_status(const char *text)
{
    browser_strlcpy(g_status, sizeof(g_status), text);
    g_need_redraw = true;
}

static void browser_set_title(const char *title)
{
    if (title == NULL || title[0] == '\0') {
        browser_strlcpy(g_page_title, sizeof(g_page_title), "Browser");
        xapi_SetWindowTitle(g_window, (char *)"Browser");
        return;
    }

    browser_strlcpy(g_page_title, sizeof(g_page_title), title);
    xapi_SetWindowTitle(g_window, g_page_title);
}

static bool browser_parse_url(const char *input, browser_ParsedUrl *parsed, char *normalized, int normalized_size, char *error, int error_size)
{
    if (input == NULL || parsed == NULL || normalized == NULL) {
        browser_strlcpy(error, error_size, "bad argument");
        return false;
    }

    char local[BROWSER_URL_MAX];
    browser_strlcpy(local, sizeof(local), input);
    browser_trim_inplace(local);
    if (local[0] == '\0') {
        browser_strlcpy(error, error_size, "empty url");
        return false;
    }

    if (!browser_starts_with_nocase(local, "http://") &&
        !browser_starts_with_nocase(local, "https://")) {
        char temp[BROWSER_URL_MAX];
        snprintf(temp, sizeof(temp), "https://%s", local);
        browser_strlcpy(local, sizeof(local), temp);
    }

    memset(parsed, 0, sizeof(*parsed));
    if (browser_starts_with_nocase(local, "https://")) {
        parsed->use_tls = true;
        browser_strlcpy(parsed->scheme, sizeof(parsed->scheme), "https");
        parsed->port = 443;
    } else if (browser_starts_with_nocase(local, "http://")) {
        parsed->use_tls = false;
        browser_strlcpy(parsed->scheme, sizeof(parsed->scheme), "http");
        parsed->port = 80;
    } else {
        browser_strlcpy(error, error_size, "unsupported scheme");
        return false;
    }

    const char *host = strstr(local, "://");
    host = host == NULL ? local : host + 3;

    const char *cut = host;
    while (*cut != '\0' && *cut != '/' && *cut != '?' && *cut != '#') {
        ++cut;
    }

    char authority[BROWSER_HOST_MAX + 32];
    int authority_len = (int)(cut - host);
    if (authority_len <= 0 || authority_len >= (int)sizeof(authority)) {
        browser_strlcpy(error, error_size, "invalid host");
        return false;
    }
    memcpy(authority, host, (size_t)authority_len);
    authority[authority_len] = '\0';

    if (*cut == '/') {
        browser_strlcpy(parsed->path, sizeof(parsed->path), cut);
    } else if (*cut == '?') {
        snprintf(parsed->path, sizeof(parsed->path), "/%s", cut);
    } else {
        browser_strlcpy(parsed->path, sizeof(parsed->path), "/");
    }

    char *fragment = strchr(parsed->path, '#');
    if (fragment != NULL) {
        *fragment = '\0';
    }

    if (authority[0] == '[') {
        char *end = strchr(authority, ']');
        if (end == NULL) {
            browser_strlcpy(error, error_size, "bad ipv6 host");
            return false;
        }
        *end = '\0';
        browser_strlcpy(parsed->host, sizeof(parsed->host), authority + 1);
        if (end[1] != '\0') {
            if (end[1] != ':') {
                browser_strlcpy(error, error_size, "bad port");
                return false;
            }
            long value = strtol(end + 2, NULL, 10);
            if (value <= 0 || value > 65535) {
                browser_strlcpy(error, error_size, "bad port");
                return false;
            }
            parsed->port = (uint16_t)value;
        }
    } else {
        char *port_sep = strrchr(authority, ':');
        if (port_sep != NULL && strchr(authority, ':') == port_sep) {
            *port_sep = '\0';
            long value = strtol(port_sep + 1, NULL, 10);
            if (value <= 0 || value > 65535) {
                browser_strlcpy(error, error_size, "bad port");
                return false;
            }
            parsed->port = (uint16_t)value;
        }
        browser_strlcpy(parsed->host, sizeof(parsed->host), authority);
    }

    if (parsed->host[0] == '\0') {
        browser_strlcpy(error, error_size, "missing host");
        return false;
    }

    if (strchr(parsed->host, ':') != NULL) {
        snprintf(normalized, normalized_size, "%s://[%s]%s",
                 parsed->scheme, parsed->host, parsed->path);
    } else {
        snprintf(normalized, normalized_size, "%s://%s%s",
                 parsed->scheme, parsed->host, parsed->path);
    }
    return true;
}

static bool browser_resolve_redirect(const char *current_url,
                                     const browser_ParsedUrl *current_parsed,
                                     const char *location,
                                     char *out,
                                     int out_size)
{
    if (location == NULL || location[0] == '\0' || out == NULL || out_size <= 0) {
        return false;
    }

    char trimmed[BROWSER_URL_MAX];
    browser_strlcpy(trimmed, sizeof(trimmed), location);
    browser_trim_inplace(trimmed);
    if (trimmed[0] == '\0') {
        return false;
    }

    if (browser_starts_with_nocase(trimmed, "http://") ||
        browser_starts_with_nocase(trimmed, "https://")) {
        browser_strlcpy(out, out_size, trimmed);
        return true;
    }

    if (browser_starts_with_nocase(trimmed, "//")) {
        snprintf(out, out_size, "%s:%s", current_parsed->scheme, trimmed);
        return true;
    }

    if (trimmed[0] == '/') {
        if (strchr(current_parsed->host, ':') != NULL) {
            snprintf(out, out_size, "%s://[%s]%s",
                     current_parsed->scheme, current_parsed->host, trimmed);
        } else {
            snprintf(out, out_size, "%s://%s%s",
                     current_parsed->scheme, current_parsed->host, trimmed);
        }
        return true;
    }

    char base_dir[BROWSER_PATH_MAX];
    browser_strlcpy(base_dir, sizeof(base_dir), current_parsed->path);
    char *query = strchr(base_dir, '?');
    if (query != NULL) {
        *query = '\0';
    }

    char *last_slash = strrchr(base_dir, '/');
    if (last_slash == NULL) {
        browser_strlcpy(base_dir, sizeof(base_dir), "/");
    } else {
        last_slash[1] = '\0';
    }

    if (trimmed[0] == '?') {
        char base_path[BROWSER_PATH_MAX];
        browser_strlcpy(base_path, sizeof(base_path), current_parsed->path);
        char *base_query = strchr(base_path, '?');
        if (base_query != NULL) {
            *base_query = '\0';
        }
        if (strchr(current_parsed->host, ':') != NULL) {
            snprintf(out, out_size, "%s://[%s]%s%s",
                     current_parsed->scheme, current_parsed->host, base_path, trimmed);
        } else {
            snprintf(out, out_size, "%s://%s%s%s",
                     current_parsed->scheme, current_parsed->host, base_path, trimmed);
        }
        return true;
    }

    if (strchr(current_parsed->host, ':') != NULL) {
        snprintf(out, out_size, "%s://[%s]%s%s",
                 current_parsed->scheme, current_parsed->host, base_dir, trimmed);
    } else {
        snprintf(out, out_size, "%s://%s%s%s",
                 current_parsed->scheme, current_parsed->host, base_dir, trimmed);
    }
    (void)current_url;
    return true;
}

static int browser_hex_value(char ch)
{
    if (ch >= '0' && ch <= '9') {
        return ch - '0';
    }
    ch = browser_to_lower(ch);
    if (ch >= 'a' && ch <= 'f') {
        return ch - 'a' + 10;
    }
    return -1;
}

static bool browser_decode_chunked(const char *data, int len, browser_Buffer *output)
{
    browser_buffer_reset(output);
    if (data == NULL || len <= 0) {
        return true;
    }

    int pos = 0;
    while (pos < len) {
        int line_end = pos;
        while (line_end < len && data[line_end] != '\n') {
            ++line_end;
        }
        if (line_end >= len) {
            return false;
        }

        char line[64];
        int line_len = browser_min((int)sizeof(line) - 1, line_end - pos);
        memcpy(line, data + pos, (size_t)line_len);
        line[line_len] = '\0';
        browser_trim_inplace(line);

        int chunk_size = 0;
        int digit_count = 0;
        for (int i = 0; line[i] != '\0'; ++i) {
            if (line[i] == ';') {
                break;
            }
            int value = browser_hex_value(line[i]);
            if (value < 0) {
                return false;
            }
            chunk_size = chunk_size * 16 + value;
            digit_count++;
        }
        if (digit_count <= 0) {
            return false;
        }

        pos = line_end + 1;
        if (chunk_size == 0) {
            return true;
        }
        if (pos + chunk_size > len) {
            return false;
        }
        if (!browser_buffer_append(output, data + pos, chunk_size, BROWSER_RAW_LIMIT)) {
            return false;
        }
        pos += chunk_size;
        if (pos < len && data[pos] == '\r') {
            ++pos;
        }
        if (pos < len && data[pos] == '\n') {
            ++pos;
        }
    }

    return false;
}

static bool browser_extract_charset(const char *content_type, char *charset, int charset_size)
{
    charset[0] = '\0';
    if (content_type == NULL) {
        return false;
    }

    const char *found = strstr(content_type, "charset=");
    if (found == NULL) {
        found = strstr(content_type, "Charset=");
    }
    if (found == NULL) {
        return false;
    }
    found += 8;

    char temp[64];
    int i = 0;
    if (*found == '"' || *found == '\'') {
        char quote = *found++;
        while (*found != '\0' && *found != quote && i < (int)sizeof(temp) - 1) {
            temp[i++] = *found++;
        }
    } else {
        while (*found != '\0' && *found != ';' && !browser_is_space(*found) && i < (int)sizeof(temp) - 1) {
            temp[i++] = *found++;
        }
    }
    temp[i] = '\0';
    browser_trim_inplace(temp);
    browser_strlcpy(charset, charset_size, temp);
    return temp[0] != '\0';
}

static bool browser_meta_is_html(const char *content_type, const char *body)
{
    if (content_type != NULL) {
        if (browser_find_nocase(content_type, "text/html", 0) >= 0 ||
            browser_find_nocase(content_type, "application/xhtml+xml", 0) >= 0) {
            return true;
        }
        if (browser_find_nocase(content_type, "text/plain", 0) >= 0) {
            return false;
        }
    }

    if (body == NULL) {
        return false;
    }

    return browser_find_nocase(body, "<html", 0) >= 0 ||
           browser_find_nocase(body, "<!doctype html", 0) >= 0 ||
           browser_find_nocase(body, "<body", 0) >= 0;
}

static bool browser_parse_response(browser_ResponseMeta *meta)
{
    if (meta == NULL || g_response_buffer.data == NULL || g_response_buffer.length <= 0) {
        return false;
    }

    memset(meta, 0, sizeof(*meta));
    meta->status_code = 0;
    meta->body = g_response_buffer.data;
    meta->body_len = g_response_buffer.length;

    char *header_end = strstr(g_response_buffer.data, "\r\n\r\n");
    int delimiter = 4;
    if (header_end == NULL) {
        header_end = strstr(g_response_buffer.data, "\n\n");
        delimiter = 2;
    }

    if (header_end != NULL) {
        int header_len = (int)(header_end - g_response_buffer.data);
        char *body = header_end + delimiter;

        char line[1024];
        int line_pos = 0;
        int first_line = 1;
        for (int i = 0; i <= header_len; ++i) {
            char ch = (i == header_len) ? '\n' : g_response_buffer.data[i];
            if (ch == '\r') {
                continue;
            }
            if (ch != '\n' && line_pos < (int)sizeof(line) - 1) {
                line[line_pos++] = ch;
                continue;
            }

            line[line_pos] = '\0';
            line_pos = 0;
            if (line[0] == '\0') {
                continue;
            }

            if (first_line) {
                first_line = 0;
                char *space = strchr(line, ' ');
                if (space != NULL) {
                    meta->status_code = (int)strtol(space + 1, NULL, 10);
                }
                continue;
            }

            char *colon = strchr(line, ':');
            if (colon == NULL) {
                continue;
            }
            *colon = '\0';
            char *value = colon + 1;
            browser_trim_inplace(line);
            browser_trim_inplace(value);

            if (browser_stricmp(line, "Content-Type") == 0) {
                browser_strlcpy(meta->content_type, sizeof(meta->content_type), value);
                browser_extract_charset(value, meta->charset, sizeof(meta->charset));
            } else if (browser_stricmp(line, "Location") == 0) {
                browser_strlcpy(meta->location, sizeof(meta->location), value);
            } else if (browser_stricmp(line, "Transfer-Encoding") == 0) {
                if (browser_find_nocase(value, "chunked", 0) >= 0) {
                    meta->chunked = true;
                }
            }
        }

        meta->body = body;
        meta->body_len = g_response_buffer.length - (int)(body - g_response_buffer.data);
    }

    if (meta->chunked) {
        if (!browser_decode_chunked(meta->body, meta->body_len, &g_chunked_buffer)) {
            return false;
        }
        meta->body = g_chunked_buffer.data;
        meta->body_len = g_chunked_buffer.length;
    }

    if (meta->body != NULL) {
        meta->body[meta->body_len] = '\0';
    }
    meta->html_like = browser_meta_is_html(meta->content_type, meta->body);
    return true;
}

static void browser_text_append_space(browser_Buffer *out)
{
    if (out == NULL || out->length <= 0) {
        return;
    }
    char last = out->data[out->length - 1];
    if (last == ' ' || last == '\n') {
        return;
    }
    browser_buffer_append_char(out, ' ', BROWSER_TEXT_LIMIT);
}

static void browser_text_append_newline(browser_Buffer *out)
{
    if (out == NULL) {
        return;
    }
    if (out->length <= 0) {
        return;
    }
    if (out->data[out->length - 1] == '\n') {
        return;
    }
    browser_buffer_append_char(out, '\n', BROWSER_TEXT_LIMIT);
}

static void browser_text_append_paragraph(browser_Buffer *out)
{
    if (out == NULL || out->length <= 0) {
        return;
    }
    if (out->data[out->length - 1] != '\n') {
        browser_buffer_append_char(out, '\n', BROWSER_TEXT_LIMIT);
    }
    if (out->length > 0 && out->data[out->length - 1] == '\n' &&
        (out->length < 2 || out->data[out->length - 2] != '\n')) {
        browser_buffer_append_char(out, '\n', BROWSER_TEXT_LIMIT);
    }
}

static int browser_decode_entity(const char *text, int pos, char *out_char)
{
    if (text[pos] != '&') {
        return 0;
    }

    if (strncmp(text + pos, "&amp;", 5) == 0) {
        *out_char = '&';
        return 5;
    }
    if (strncmp(text + pos, "&lt;", 4) == 0) {
        *out_char = '<';
        return 4;
    }
    if (strncmp(text + pos, "&gt;", 4) == 0) {
        *out_char = '>';
        return 4;
    }
    if (strncmp(text + pos, "&quot;", 6) == 0) {
        *out_char = '"';
        return 6;
    }
    if (strncmp(text + pos, "&apos;", 6) == 0) {
        *out_char = '\'';
        return 6;
    }
    if (strncmp(text + pos, "&nbsp;", 6) == 0) {
        *out_char = ' ';
        return 6;
    }
    if (strncmp(text + pos, "&#", 2) == 0) {
        int index = pos + 2;
        int base = 10;
        if (text[index] == 'x' || text[index] == 'X') {
            base = 16;
            ++index;
        }
        int value = 0;
        int digits = 0;
        while (text[index] != '\0' && text[index] != ';') {
            int digit = base == 16 ? browser_hex_value(text[index])
                                   : (text[index] >= '0' && text[index] <= '9' ? text[index] - '0' : -1);
            if (digit < 0) {
                return 0;
            }
            value = value * base + digit;
            ++digits;
            ++index;
        }
        if (digits > 0 && text[index] == ';') {
            *out_char = (value >= 32 && value <= 126) ? (char)value : '?';
            return index - pos + 1;
        }
    }

    return 0;
}

static int browser_parse_tag_name(const char *html, int start, int tag_end, char *name, int name_size, bool *closing)
{
    if (name == NULL || name_size <= 0) {
        return 0;
    }

    int pos = start + 1;
    while (pos < tag_end && browser_is_space(html[pos])) {
        ++pos;
    }

    *closing = false;
    if (pos < tag_end && html[pos] == '/') {
        *closing = true;
        ++pos;
    }

    while (pos < tag_end && browser_is_space(html[pos])) {
        ++pos;
    }

    int n = 0;
    while (pos < tag_end) {
        char ch = html[pos];
        if (browser_is_space(ch) || ch == '/' || ch == '>') {
            break;
        }
        if (n < name_size - 1) {
            name[n++] = browser_to_lower(ch);
        }
        ++pos;
    }
    name[n] = '\0';
    return n;
}

static bool browser_tag_is_block(const char *name)
{
    return browser_stricmp(name, "p") == 0 ||
           browser_stricmp(name, "div") == 0 ||
           browser_stricmp(name, "section") == 0 ||
           browser_stricmp(name, "article") == 0 ||
           browser_stricmp(name, "header") == 0 ||
           browser_stricmp(name, "footer") == 0 ||
           browser_stricmp(name, "nav") == 0 ||
           browser_stricmp(name, "main") == 0 ||
           browser_stricmp(name, "aside") == 0 ||
           browser_stricmp(name, "h1") == 0 ||
           browser_stricmp(name, "h2") == 0 ||
           browser_stricmp(name, "h3") == 0 ||
           browser_stricmp(name, "h4") == 0 ||
           browser_stricmp(name, "h5") == 0 ||
           browser_stricmp(name, "h6") == 0 ||
           browser_stricmp(name, "pre") == 0 ||
           browser_stricmp(name, "table") == 0 ||
           browser_stricmp(name, "tr") == 0 ||
           browser_stricmp(name, "ul") == 0 ||
           browser_stricmp(name, "ol") == 0 ||
           browser_stricmp(name, "blockquote") == 0;
}

static void browser_extract_html_title(const char *html, char *out, int out_size)
{
    browser_strlcpy(out, out_size, "");
    if (html == NULL) {
        return;
    }

    int title_start = browser_find_nocase(html, "<title", 0);
    if (title_start < 0) {
        return;
    }
    char *open_end = strchr((char *)html + title_start, '>');
    if (open_end == NULL) {
        return;
    }
    int content_start = (int)(open_end - html) + 1;
    int title_end = browser_find_nocase(html, "</title", content_start);
    if (title_end < 0) {
        return;
    }

    browser_Buffer title_buffer = {0};
    int span_len = title_end - content_start;
    if (span_len <= 0) {
        return;
    }
    if (!browser_buffer_reserve(&title_buffer, span_len + 4, span_len + 8)) {
        return;
    }
    title_buffer.length = 0;
    for (int i = content_start; i < title_end; ++i) {
        char ch = html[i];
        if (ch == '<') {
            char *tag_end = strchr((char *)html + i, '>');
            if (tag_end == NULL || tag_end >= html + title_end) {
                break;
            }
            i = (int)(tag_end - html);
            continue;
        }
        if (browser_is_space(ch)) {
            browser_text_append_space(&title_buffer);
            continue;
        }
        if (ch == '&') {
            char entity = 0;
            int consumed = browser_decode_entity(html, i, &entity);
            if (consumed > 0) {
                browser_buffer_append_char(&title_buffer, entity, span_len + 8);
                i += consumed - 1;
                continue;
            }
        }
        browser_buffer_append_char(&title_buffer, ch, span_len + 8);
    }
    if (title_buffer.data != NULL) {
        browser_trim_inplace(title_buffer.data);
        browser_strlcpy(out, out_size, title_buffer.data);
        free(title_buffer.data);
    }
}

static bool browser_html_to_text(const char *html)
{
    browser_buffer_reset(&g_text_buffer);
    if (html == NULL) {
        return false;
    }

    int len = (int)strlen(html);
    for (int i = 0; i < len; ++i) {
        char ch = html[i];
        if (ch == '<') {
            if (i + 3 < len && strncmp(html + i, "<!--", 4) == 0) {
                char *comment_end = strstr((char *)html + i + 4, "-->");
                if (comment_end == NULL) {
                    break;
                }
                i = (int)(comment_end - html) + 2;
                continue;
            }

            char *tag_end_ptr = strchr((char *)html + i, '>');
            if (tag_end_ptr == NULL) {
                break;
            }
            int tag_end = (int)(tag_end_ptr - html);
            char tag_name[32];
            bool closing = false;
            browser_parse_tag_name(html, i, tag_end, tag_name, sizeof(tag_name), &closing);

            if (!closing &&
                (browser_stricmp(tag_name, "script") == 0 ||
                 browser_stricmp(tag_name, "style") == 0 ||
                 browser_stricmp(tag_name, "head") == 0 ||
                 browser_stricmp(tag_name, "svg") == 0)) {
                char close_tag[48];
                snprintf(close_tag, sizeof(close_tag), "</%s", tag_name);
                int close_pos = browser_find_nocase(html, close_tag, tag_end + 1);
                if (close_pos < 0) {
                    break;
                }
                char *close_end = strchr((char *)html + close_pos, '>');
                if (close_end == NULL) {
                    break;
                }
                i = (int)(close_end - html);
                continue;
            }

            if (browser_stricmp(tag_name, "br") == 0) {
                browser_text_append_newline(&g_text_buffer);
            } else if (browser_stricmp(tag_name, "li") == 0 && !closing) {
                browser_text_append_newline(&g_text_buffer);
                browser_buffer_append(&g_text_buffer, "* ", 2, BROWSER_TEXT_LIMIT);
            } else if (browser_tag_is_block(tag_name)) {
                browser_text_append_paragraph(&g_text_buffer);
            }

            i = tag_end;
            continue;
        }

        if (browser_is_space(ch)) {
            browser_text_append_space(&g_text_buffer);
            continue;
        }

        if (ch == '&') {
            char decoded = 0;
            int consumed = browser_decode_entity(html, i, &decoded);
            if (consumed > 0) {
                if (decoded == ' ') {
                    browser_text_append_space(&g_text_buffer);
                } else {
                    browser_buffer_append_char(&g_text_buffer, decoded, BROWSER_TEXT_LIMIT);
                }
                i += consumed - 1;
                continue;
            }
        }

        browser_buffer_append_char(&g_text_buffer, ch, BROWSER_TEXT_LIMIT);
    }

    return g_text_buffer.data != NULL;
}

static void browser_wrap_text(const char *text)
{
    browser_clear_lines();
    if (text == NULL || text[0] == '\0') {
        browser_append_line_direct("(empty document)");
        return;
    }

    char line[BROWSER_LINE_TEXT_MAX];
    char word[BROWSER_LINE_TEXT_MAX];
    int line_len = 0;
    int word_len = 0;
    int char_limit = BROWSER_LINE_TEXT_MAX - 6;

    line[0] = '\0';
    word[0] = '\0';

    for (const char *p = text;; ++p) {
        char ch = *p;
        bool is_break = (ch == '\0' || ch == '\n' || ch == ' ' || ch == '\t' || ch == '\r');

        if (!is_break) {
            if (word_len < BROWSER_LINE_TEXT_MAX - 1) {
                word[word_len++] = ch;
                word[word_len] = '\0';
            }
            continue;
        }

        if (word_len > 0) {
            int need = word_len + (line_len > 0 ? 1 : 0);
            if (line_len > 0 && line_len + need > char_limit) {
                browser_append_line_direct(line);
                line_len = 0;
                line[0] = '\0';
            }

            if (line_len > 0) {
                line[line_len++] = ' ';
                line[line_len] = '\0';
            }

            if (word_len >= char_limit) {
                int word_pos = 0;
                while (word_pos < word_len) {
                    int take = browser_min(char_limit, word_len - word_pos);
                    char chunk[BROWSER_LINE_TEXT_MAX];
                    memcpy(chunk, word + word_pos, (size_t)take);
                    chunk[take] = '\0';
                    if (line_len > 0) {
                        browser_append_line_direct(line);
                        line_len = 0;
                        line[0] = '\0';
                    }
                    browser_append_line_direct(chunk);
                    word_pos += take;
                }
            } else {
                memcpy(line + line_len, word, (size_t)word_len);
                line_len += word_len;
                line[line_len] = '\0';
            }

            word_len = 0;
            word[0] = '\0';
        }

        if (ch == '\n' || ch == '\0') {
            if (line_len > 0) {
                browser_append_line_direct(line);
                line_len = 0;
                line[0] = '\0';
            } else if (ch == '\n') {
                browser_append_blank_line();
            }
        }

        if (ch == '\0') {
            break;
        }
    }

    if (g_line_count <= 0) {
        browser_append_line_direct("(empty document)");
    }
    browser_clamp_scroll();
}

static void browser_show_text_document(const char *title, const char *text)
{
    browser_reset_rich_document();
    browser_set_title(title);
    browser_wrap_text(text);
}

static void browser_show_binary_document(const char *content_type, int body_len)
{
    char message[256];
    snprintf(message, sizeof(message),
             "Unsupported document type.\n\nContent-Type: %s\nBody bytes: %d\n\nThis browser currently renders HTML/CSS documents and plain text.",
             content_type != NULL && content_type[0] != '\0' ? content_type : "(unknown)",
             body_len);
    browser_show_text_document("Binary Response", message);
}

static void browser_show_error_document(const char *title, const char *message)
{
    char text[512];
    snprintf(text, sizeof(text), "%s\n\n%s", title, message == NULL ? "request failed" : message);
    browser_show_text_document(title, text);
}

static void browser_render_window()
{
    const int panel_y1 = BROWSER_HEADER_HEIGHT;
    const int panel_y2 = BROWSER_WINDOW_HEIGHT - BROWSER_PADDING;
    const int panel_x1 = BROWSER_PADDING;
    const int panel_x2 = BROWSER_WINDOW_WIDTH - BROWSER_PADDING;
    const int url_x1 = 78;
    const int url_y1 = 18;
    const int url_x2 = BROWSER_WINDOW_WIDTH - 126;
    const int url_y2 = 46;
    const int go_x1 = BROWSER_WINDOW_WIDTH - 106;
    const int go_y1 = 18;
    const int go_x2 = BROWSER_WINDOW_WIDTH - 24;
    const int go_y2 = 46;
    const int visible = browser_visible_lines();

    xapi_DrawRect(g_window, 0, 0, BROWSER_WINDOW_WIDTH - 1, BROWSER_WINDOW_HEIGHT - 1, BROWSER_COLOR_BG, true);
    xapi_DrawRect(g_window, 0, 0, BROWSER_WINDOW_WIDTH - 1, BROWSER_HEADER_HEIGHT - 1, BROWSER_COLOR_HEADER, true);
    xapi_DrawRect(g_window, panel_x1, panel_y1, panel_x2, panel_y2, BROWSER_COLOR_PANEL, true);

    xapi_DrawSWText(g_window, 14, 14, (char *)"Browser", 0xffffffff);
    xapi_DrawSWText(g_window, 14, 34, g_page_title, 0xdce6f2ff);
    xapi_DrawSWText(g_window, 14, 54, g_status, BROWSER_COLOR_STATUS);

    xapi_DrawRect(g_window, url_x1, url_y1, url_x2, url_y2, BROWSER_COLOR_INPUT, true);
    xapi_DrawRect(g_window, url_x1, url_y1, url_x2, url_y2, BROWSER_COLOR_INPUT_BORDER, false);
    xapi_DrawSWText(g_window, 14, 24, (char *)"URL", 0xffffffff);
    xapi_DrawSWText(g_window, url_x1 + 8, url_y1 + 8, g_url_input, BROWSER_COLOR_TEXT);

    xapi_DrawRect(g_window, go_x1, go_y1, go_x2, go_y2, BROWSER_COLOR_BUTTON, true);
    xapi_DrawSWText(g_window, go_x1 + 24, go_y1 + 8, (char *)"Go", BROWSER_COLOR_BUTTON_TEXT);

    xapi_DrawSWText(g_window,
                    panel_x1 + 10,
                    panel_y1 + 8,
                    (char *)"Enter: load   Mouse wheel: scroll   Click URL bar to edit",
                    BROWSER_COLOR_MUTED);

    if (g_rich_document && g_css_document != NULL) {
        browser_css_draw(g_css_document,
                         g_window,
                         browser_content_x1(),
                         browser_content_y1(),
                         g_scroll_pixels,
                         browser_content_width(),
                         browser_content_height());
    } else {
        const int start_line = g_scroll_top;
        const int y = browser_content_y1();
        for (int i = 0; i < visible && start_line + i < g_line_count; ++i) {
            xapi_DrawSWText(g_window, browser_content_x1(), y + i * 16, g_lines[start_line + i], BROWSER_COLOR_TEXT);
        }
    }

    if ((g_rich_document && browser_max_scroll() > 0) || (!g_rich_document && g_line_count > visible)) {
        char scroll_info[64];
        int scroll_now = g_rich_document ? g_scroll_pixels : g_scroll_top;
        snprintf(scroll_info, sizeof(scroll_info), "%d/%d", scroll_now + 1, browser_max_scroll() + 1);
        xapi_DrawSWText(g_window, panel_x2 - 56, panel_y1 + 8, scroll_info, BROWSER_COLOR_MUTED);
    }

    xapi_RefreshWindow(g_window);
    g_need_redraw = false;
}

static bool browser_fetch_once(const char *url, browser_ResponseMeta *meta, char *normalized, int normalized_size, char *error, int error_size)
{
    browser_ParsedUrl parsed;
    if (!browser_parse_url(url, &parsed, normalized, normalized_size, error, error_size)) {
        return false;
    }

    browser_buffer_reset(&g_response_buffer);
    browser_buffer_reset(&g_chunked_buffer);

    xhttp_request_t request;
    request.method = "GET";
    request.host = parsed.host;
    request.port = parsed.port;
    request.path = parsed.path;
    request.body = NULL;
    request.content_type = NULL;
    request.extra_headers =
        "User-Agent: XJ380-browser/1.0\r\n"
        "Accept: text/html, text/plain, */*\r\n"
        "Accept-Encoding: identity\r\n";

    int status = parsed.use_tls
                     ? xtls_request(&request, browser_response_append, &g_response_buffer)
                     : xhttp_request(&request, browser_response_append, &g_response_buffer);
    if (status != XHTTP_OK) {
        switch (status) {
        case XHTTP_ERR_BAD_HOST: browser_strlcpy(error, error_size, "dns failed"); break;
        case XHTTP_ERR_SOCKET: browser_strlcpy(error, error_size, "socket failed"); break;
        case XHTTP_ERR_CONNECT: browser_strlcpy(error, error_size, "connect failed"); break;
        case XHTTP_ERR_SEND:
        case XHTTP_ERR_TLS_SEND: browser_strlcpy(error, error_size, "send failed"); break;
        case XHTTP_ERR_RECV:
        case XHTTP_ERR_TLS_RECV: browser_strlcpy(error, error_size, "recv failed"); break;
        case XHTTP_ERR_TIMEOUT: browser_strlcpy(error, error_size, "timeout"); break;
        case XHTTP_ERR_TLS_INIT: browser_strlcpy(error, error_size, "tls init failed"); break;
        case XHTTP_ERR_TLS_SEED: browser_strlcpy(error, error_size, "tls seed failed"); break;
        case XHTTP_ERR_TLS_CONFIG: browser_strlcpy(error, error_size, "tls config failed"); break;
        case XHTTP_ERR_TLS_HANDSHAKE: browser_strlcpy(error, error_size, "tls handshake failed"); break;
        default: browser_strlcpy(error, error_size, "request failed"); break;
        }
        return false;
    }

    if (!browser_parse_response(meta)) {
        browser_strlcpy(error, error_size, "bad response");
        return false;
    }

    if (g_response_buffer.truncated || g_chunked_buffer.truncated) {
        browser_set_status("response truncated");
    }
    return true;
}

static bool browser_resolve_url_for_fetch(const char *url, const char *base_url, char *resolved, int resolved_size)
{
    browser_ParsedUrl parsed;
    char              normalized[BROWSER_URL_MAX];
    char              error[128];

    if (url == NULL || resolved == NULL || resolved_size <= 0) {
        return false;
    }

    if (browser_starts_with_nocase(url, "http://") || browser_starts_with_nocase(url, "https://")) {
        browser_strlcpy(resolved, resolved_size, url);
        return true;
    }

    if (base_url == NULL || base_url[0] == '\0') {
        return false;
    }

    if (!browser_parse_url(base_url, &parsed, normalized, sizeof(normalized), error, sizeof(error))) {
        return false;
    }

    return browser_resolve_redirect(normalized, &parsed, url, resolved, resolved_size);
}

static int browser_css_fetch_resource(void *user_data,
                                      const char *url,
                                      const char *base_url,
                                         char **body_out,
                                         int *body_len_out,
                                         char *final_url_out,
                                         int final_url_out_size)
{
    (void)user_data;

    browser_ResponseMeta meta;
    char                 resolved[BROWSER_URL_MAX];
    char                 normalized[BROWSER_URL_MAX];
    char                 error[128];
    char                *copy = NULL;

    if (body_out != NULL) {
        *body_out = NULL;
    }
    if (body_len_out != NULL) {
        *body_len_out = 0;
    }
    if (final_url_out != NULL && final_url_out_size > 0) {
        final_url_out[0] = '\0';
    }

    if (url == NULL || body_out == NULL || body_len_out == NULL ||
        !browser_resolve_url_for_fetch(url, base_url, resolved, sizeof(resolved))) {
        return 0;
    }

    if (!browser_fetch_once(resolved, &meta, normalized, sizeof(normalized), error, sizeof(error))) {
        return 0;
    }

    if (meta.body_len > 0) {
        copy = (char *)malloc((size_t)meta.body_len + 1);
        if (copy == NULL) {
            return 0;
        }
        memcpy(copy, meta.body, (size_t)meta.body_len);
        copy[meta.body_len] = '\0';
    }

    *body_out = copy;
    *body_len_out = meta.body_len;
    if (final_url_out != NULL && final_url_out_size > 0) {
        browser_strlcpy(final_url_out, final_url_out_size, normalized);
    }
    return 1;
}

static void browser_css_request_navigation(void *user_data, const char *url, const char *base_url)
{
    char resolved[BROWSER_URL_MAX];

    (void)user_data;

    if (url == NULL || url[0] == '\0' ||
        !browser_resolve_url_for_fetch(url, base_url, resolved, sizeof(resolved))) {
        return;
    }

    browser_strlcpy(g_url_input, sizeof(g_url_input), resolved);
    g_url_input_len = (int)strlen(g_url_input);
    g_input_focus = false;
    g_fetch_requested = true;
    browser_set_status("link selected");
}

static bool browser_try_render_html(const char *html, const char *base_url, const char *charset)
{
    const char *title = NULL;
    (void)charset;

    browser_reset_rich_document();
    browser_clear_lines();

    if (g_css_document == NULL || html == NULL || html[0] == '\0') {
        return false;
    }

    if (!browser_css_load_html(g_css_document,
                               html,
                               base_url,
                               browser_content_width(),
                               browser_content_height())) {
        browser_reset_rich_document();
        return false;
    }

    g_rich_document = true;
    g_rich_content_height = browser_css_height(g_css_document);
    g_scroll_pixels = 0;
    g_mouse_in_content = false;
    title = browser_css_title(g_css_document);
    browser_set_title(title != NULL && title[0] != '\0' ? title : base_url);
    browser_clamp_scroll();
    return true;
}

static void browser_navigate(const char *requested_url)
{
    char current_url[BROWSER_URL_MAX];
    char next_url[BROWSER_URL_MAX];
    char error[128];
    char title[BROWSER_TITLE_MAX];
    char *html_copy = NULL;
    browser_ParsedUrl parsed;
    browser_ResponseMeta meta;

    browser_strlcpy(next_url, sizeof(next_url), requested_url);
    browser_trim_inplace(next_url);
    if (next_url[0] == '\0') {
        browser_set_status("empty url");
        browser_show_error_document("Navigation Error", "URL is empty.");
        return;
    }

    browser_set_status("loading...");
    browser_render_window();

    for (int redirect = 0; redirect <= BROWSER_MAX_REDIRECTS; ++redirect) {
        if (!browser_fetch_once(next_url, &meta, current_url, sizeof(current_url), error, sizeof(error))) {
            browser_set_status(error);
            browser_show_error_document("Request Failed", error);
            return;
        }

        if ((meta.status_code == 301 || meta.status_code == 302 || meta.status_code == 303 ||
             meta.status_code == 307 || meta.status_code == 308) &&
            meta.location[0] != '\0') {
            if (redirect == BROWSER_MAX_REDIRECTS) {
                browser_set_status("redirect loop");
                browser_show_error_document("Redirect Error", "Too many redirects.");
                return;
            }

            if (!browser_parse_url(current_url, &parsed, current_url, sizeof(current_url), error, sizeof(error)) ||
                !browser_resolve_redirect(current_url, &parsed, meta.location, next_url, sizeof(next_url))) {
                browser_set_status("bad redirect");
                browser_show_error_document("Redirect Error", "Failed to resolve redirect URL.");
                return;
            }

            browser_set_status("redirecting...");
            browser_render_window();
            continue;
        }

        browser_strlcpy(g_current_url, sizeof(g_current_url), current_url);
        browser_strlcpy(g_url_input, sizeof(g_url_input), current_url);
        g_url_input_len = (int)strlen(g_url_input);

        if (meta.html_like) {
            browser_extract_html_title(meta.body, title, sizeof(title));

            if (meta.body != NULL && meta.body_len >= 0) {
                html_copy = (char *)malloc((size_t)meta.body_len + 1);
                if (html_copy != NULL) {
                    memcpy(html_copy, meta.body, (size_t)meta.body_len);
                    html_copy[meta.body_len] = '\0';
                }
            }

            if (html_copy != NULL && browser_try_render_html(html_copy, current_url, meta.charset)) {
                free(html_copy);
                html_copy = NULL;
            } else {
                browser_reset_rich_document();
                if (html_copy == NULL || !browser_html_to_text(html_copy)) {
                    if (html_copy != NULL) {
                        free(html_copy);
                    }
                    browser_set_status("html parse failed");
                    browser_show_error_document("Parse Error", "Failed to render HTML document.");
                    return;
                }
                browser_show_text_document(title[0] != '\0' ? title : current_url,
                                           g_text_buffer.data != NULL ? g_text_buffer.data : "");
                if (html_copy != NULL) {
                    free(html_copy);
                }
            }
        } else if (browser_find_nocase(meta.content_type, "text/plain", 0) >= 0 || meta.content_type[0] == '\0') {
            browser_show_text_document(current_url, meta.body != NULL ? meta.body : "");
        } else {
            browser_show_binary_document(meta.content_type, meta.body_len);
        }

        {
            char status[96];
            snprintf(status, sizeof(status), "HTTP %d", meta.status_code > 0 ? meta.status_code : 200);
            browser_set_status(status);
        }
        browser_clamp_scroll();
        return;
    }
}

static int browser_url_x1()
{
    return 78;
}

static int browser_url_x2()
{
    return BROWSER_WINDOW_WIDTH - 126;
}

static int browser_url_y1()
{
    return 18;
}

static int browser_url_y2()
{
    return 46;
}

static int browser_go_x1()
{
    return BROWSER_WINDOW_WIDTH - 106;
}

static int browser_go_x2()
{
    return BROWSER_WINDOW_WIDTH - 24;
}

static int browser_go_y1()
{
    return 18;
}

static int browser_go_y2()
{
    return 46;
}

static bool browser_point_in_rect(int x, int y, int x1, int y1, int x2, int y2)
{
    return x >= x1 && x <= x2 && y >= y1 && y <= y2;
}

static bool browser_point_in_content(int x, int y)
{
    return browser_point_in_rect(x, y, browser_content_x1(), browser_content_y1(), browser_content_x2(), browser_content_y2());
}

static void browser_handle_mouse_move(int x, int y)
{
    bool inside_content = browser_point_in_content(x, y);

    if (!g_rich_document || g_css_document == NULL) {
        g_mouse_in_content = inside_content;
        return;
    }

    if (inside_content) {
        if (browser_css_mouse_move(g_css_document,
                                   x - browser_content_x1(),
                                   y - browser_content_y1(),
                                   g_scroll_pixels)) {
            g_need_redraw = true;
        }
    } else if (g_mouse_in_content) {
        if (browser_css_mouse_leave(g_css_document)) {
            g_need_redraw = true;
        }
    }

    g_mouse_in_content = inside_content;
}

static void browser_handle_left_click(int x, int y)
{
    if (browser_point_in_rect(x, y, browser_url_x1(), browser_url_y1(), browser_url_x2(), browser_url_y2())) {
        g_input_focus = true;
        g_need_redraw = true;
        return;
    }

    if (browser_point_in_rect(x, y, browser_go_x1(), browser_go_y1(), browser_go_x2(), browser_go_y2())) {
        g_fetch_requested = true;
        g_input_focus = false;
        return;
    }

    if (browser_point_in_content(x, y)) {
        g_input_focus = false;
        if (g_rich_document && g_css_document != NULL) {
            browser_css_mouse_click(g_css_document,
                                    x - browser_content_x1(),
                                    y - browser_content_y1(),
                                    g_scroll_pixels);
            g_need_redraw = true;
        }
        return;
    }

    g_input_focus = false;
}

static void browser_handle_char(char ch)
{
    if (!g_input_focus) {
        if (ch == 'j' || ch == 'J') {
            browser_scroll_by(1);
            g_need_redraw = true;
        } else if (ch == 'k' || ch == 'K') {
            browser_scroll_by(-1);
            g_need_redraw = true;
        }
        return;
    }

    if (!browser_is_printable(ch)) {
        return;
    }
    if (g_url_input_len >= (int)sizeof(g_url_input) - 1) {
        return;
    }

    g_url_input[g_url_input_len++] = ch;
    g_url_input[g_url_input_len] = '\0';
    g_need_redraw = true;
}

static void browser_handle_special_char(char ch)
{
    if (ch == '\b') {
        if (g_input_focus && g_url_input_len > 0) {
            g_url_input[--g_url_input_len] = '\0';
            g_need_redraw = true;
        }
        return;
    }

    if (ch == '\n') {
        g_fetch_requested = true;
    }
}

static void browser_message_proc(UINT64 type, UINT64 hData, UINT64 lData)
{
    switch (type) {
    case MSG_MOVE:
        browser_handle_mouse_move((int)hData, (int)lData);
        break;
    case MSG_LBUTTON:
        browser_handle_left_click((int)hData, (int)lData);
        break;
    case MSG_CHAR:
        browser_handle_char((char)lData);
        break;
    case MSG_SPCHAR:
        browser_handle_special_char((char)lData);
        break;
    case MSG_ROLLER: {
        int delta = (int)lData;
        if (delta == 0) {
            delta = (int)hData;
        }
        if (delta > 0) {
            browser_scroll_by(-BROWSER_SCROLL_STEP);
            g_need_redraw = true;
        } else if (delta < 0) {
            browser_scroll_by(BROWSER_SCROLL_STEP);
            g_need_redraw = true;
        }
        break;
    }
    default:
        break;
    }
}

static void browser_bootstrap_url(int argc, char *argv[])
{
    if (argc > 0 && argv != NULL && argv[0] != NULL && argv[0][0] != '\0') {
        browser_strlcpy(g_url_input, sizeof(g_url_input), argv[0]);
        g_url_input_len = (int)strlen(g_url_input);
    }
}

static int browser_main_impl(int argc, char *argv[], char *envp[])
{
    (void)envp;
    browser_css_callbacks callbacks;

    XWINDOW window;
    window.title = (char *)"Browser";
    window.width = BROWSER_WINDOW_WIDTH;
    window.height = BROWSER_WINDOW_HEIGHT;
    window.sets = XWIN_NORMAL;

    xapi_CreateWindow(&g_window, &window);
    xapi_SetIcon(g_window, (char *)"/system/icon/terminal.png");
    SetMsgPrcor(g_window, browser_message_proc);

    callbacks.user_data = NULL;
    callbacks.fetch_resource = browser_css_fetch_resource;
    callbacks.navigate = browser_css_request_navigation;
    g_css_document = browser_css_create(&callbacks);

    browser_bootstrap_url(argc, argv);
    browser_set_title("Browser");
    browser_append_line_direct("Loading...");
    browser_render_window();
    g_fetch_requested = true;

    while (!g_need_exit) {
        if (g_fetch_requested) {
            g_fetch_requested = false;
            browser_navigate(g_url_input);
            g_need_redraw = true;
        }

        if (g_need_redraw) {
            browser_render_window();
        }
        xapi_Sleep(BROWSER_LOOP_SLEEP_MS);
    }

    free(g_response_buffer.data);
    free(g_chunked_buffer.data);
    free(g_text_buffer.data);
    browser_css_destroy(g_css_document);
    g_css_document = NULL;
    xapi_CloseWindow(g_window);
    return 0;
}

extern "C" int browser_main_cpp(int argc, char *argv[], char *envp[]) asm("_Z4mainiPPcS0_");
extern "C" int browser_main_cpp(int argc, char *argv[], char *envp[])
{
    return browser_main_impl(argc, argv, envp);
}
