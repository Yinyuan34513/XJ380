#include "./xapi/include/x3api.h"

static const int      HTTPGET_WIDTH            = 760;
static const int      HTTPGET_HEIGHT           = 520;
static const int      HTTPGET_MAX_LINES        = 160;
static const int      HTTPGET_LINE_LENGTH      = 256;
static const int      HTTPGET_VISIBLE_LINES    = 24;
static const uint32_t HTTPGET_COLOR_BG         = 0xf2efe9ff;
static const uint32_t HTTPGET_COLOR_PANEL      = 0xfffbf5ff;
static const uint32_t HTTPGET_COLOR_HEADER     = 0x1d3557ff;
static const uint32_t HTTPGET_COLOR_STATUS     = 0xd8f3dcff;
static const uint32_t HTTPGET_COLOR_TEXT       = 0x222222ff;
static const uint32_t HTTPGET_COLOR_INFO       = 0x3d405bff;
static const uint32_t HTTPGET_COLOR_ERROR      = 0xb00020ff;
static const uint32_t HTTPGET_COLOR_RESPONSE   = 0x264653ff;

typedef struct httpget_line {
    char     text[HTTPGET_LINE_LENGTH];
    uint32_t color;
} httpget_line_t;

static HDLE           g_window      = 0;
static bool           g_need_redraw = true;
static bool           g_send_req    = false;
static char           g_input[512]  = "GET 10.0.2.2 80 /";
static int            g_input_len   = 16;
static char           g_status[128] = "Ready";
static httpget_line_t g_lines[HTTPGET_MAX_LINES];
static int            g_line_count  = 0;

static void set_status(const char *text)
{
    strncpy(g_status, text, sizeof(g_status) - 1);
    g_status[sizeof(g_status) - 1] = '\0';
    g_need_redraw                  = true;
}

static void append_line(const char *text, uint32_t color)
{
    if (text == NULL) {
        return;
    }

    if (g_line_count == HTTPGET_MAX_LINES) {
        for (int i = 1; i < HTTPGET_MAX_LINES; ++i) {
            g_lines[i - 1] = g_lines[i];
        }
        g_line_count--;
    }

    strncpy(g_lines[g_line_count].text, text, HTTPGET_LINE_LENGTH - 1);
    g_lines[g_line_count].text[HTTPGET_LINE_LENGTH - 1] = '\0';
    g_lines[g_line_count].color                         = color;
    g_line_count++;
    g_need_redraw = true;
}

static void append_response_chunk(const char *data, int len)
{
    char line[HTTPGET_LINE_LENGTH];
    int  line_len = 0;

    for (int i = 0; i < len; ++i) {
        char ch = data[i];
        if (ch == '\r') {
            continue;
        }
        if (ch == '\n' || line_len >= HTTPGET_LINE_LENGTH - 1) {
            line[line_len] = '\0';
            append_line(line_len == 0 ? "" : line, HTTPGET_COLOR_RESPONSE);
            line_len = 0;
            if (ch == '\n') {
                continue;
            }
        }
        line[line_len++] = ch;
    }

    if (line_len > 0) {
        line[line_len] = '\0';
        append_line(line, HTTPGET_COLOR_RESPONSE);
    }
}

static bool parse_ipv4(const char *text, uint32_t *out_addr)
{
    uint32_t parts[4] = {0, 0, 0, 0};
    int      part     = 0;
    uint32_t value    = 0;

    for (const char *p = text; ; ++p) {
        char ch = *p;
        if (isdigit(ch)) {
            value = value * 10 + (uint32_t)(ch - '0');
            if (value > 255) {
                return false;
            }
        } else if (ch == '.' || ch == '\0') {
            if (part >= 4) {
                return false;
            }
            parts[part++] = value;
            value         = 0;
            if (ch == '\0') {
                break;
            }
        } else {
            return false;
        }
    }

    if (part != 4) {
        return false;
    }

    *out_addr = htonl((parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3]);
    return true;
}

static int socket_send_all(int sockfd, const char *data, int len)
{
    int sent_total = 0;
    while (sent_total < len) {
        int sent = write(sockfd, (char *)data + sent_total, (uint64_t)(len - sent_total));
        if (sent <= 0) {
            return -1;
        }
        sent_total += sent;
    }
    return 0;
}

static void render_window()
{
    xapi_DrawRect(g_window, 0, 0, HTTPGET_WIDTH - 1, HTTPGET_HEIGHT - 1, HTTPGET_COLOR_BG, true);
    xapi_DrawRect(g_window, 0, 0, HTTPGET_WIDTH - 1, 39, HTTPGET_COLOR_HEADER, true);
    xapi_DrawRect(g_window, 12, 52, HTTPGET_WIDTH - 13, HTTPGET_HEIGHT - 84, HTTPGET_COLOR_PANEL, true);
    xapi_DrawRect(g_window, 12, HTTPGET_HEIGHT - 64, HTTPGET_WIDTH - 13, HTTPGET_HEIGHT - 16, 0xffffffff, true);

    xapi_DrawSWText(g_window, 16, 12, (char *)"HTTP Client", 0xffffffff);
    xapi_DrawSWText(g_window, 170, 12, g_status, HTTPGET_COLOR_STATUS);
    xapi_DrawSWText(g_window, 18, 56, (char *)"Syntax: METHOD IP PORT PATH [BODY]", HTTPGET_COLOR_INFO);
    xapi_DrawSWText(g_window, 18, 72, (char *)"Example: GET 10.0.2.2 80 /", HTTPGET_COLOR_INFO);
    xapi_DrawSWText(g_window, 18, HTTPGET_HEIGHT - 56, (char *)"Request:", HTTPGET_COLOR_INFO);
    xapi_DrawSWText(g_window, 90, HTTPGET_HEIGHT - 56, g_input, HTTPGET_COLOR_TEXT);

    int start = g_line_count > HTTPGET_VISIBLE_LINES ? g_line_count - HTTPGET_VISIBLE_LINES : 0;
    for (int i = start; i < g_line_count; ++i) {
        xapi_DrawSWText(g_window, 20, 104 + (i - start) * 16, g_lines[i].text, g_lines[i].color);
    }

    xapi_RefreshWindow(g_window);
    g_need_redraw = false;
}

static void run_http_request()
{
    g_send_req = false;

    char request_line[sizeof(g_input)];
    strncpy(request_line, g_input, sizeof(request_line) - 1);
    request_line[sizeof(request_line) - 1] = '\0';

    char *method = strtok(request_line, " ");
    char *host   = strtok(NULL, " ");
    char *port_s = strtok(NULL, " ");
    char *path   = strtok(NULL, " ");
    char *body   = strtok(NULL, "");

    if (method == NULL || host == NULL || port_s == NULL || path == NULL) {
        append_line("Invalid syntax. Use: METHOD IP PORT PATH [BODY]", HTTPGET_COLOR_ERROR);
        set_status("parse failed");
        return;
    }

    uint32_t addr = 0;
    if (!parse_ipv4(host, &addr)) {
        append_line("Invalid IPv4 address.", HTTPGET_COLOR_ERROR);
        set_status("bad ip");
        return;
    }

    long port = strtol(port_s, NULL, 10);
    if (port <= 0 || port > 65535) {
        append_line("Invalid port.", HTTPGET_COLOR_ERROR);
        set_status("bad port");
        return;
    }

    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        append_line("socket() failed", HTTPGET_COLOR_ERROR);
        set_status("socket failed");
        return;
    }

    struct sockaddr_in addr_in;
    memset(&addr_in, 0, sizeof(addr_in));
    addr_in.sin_family      = AF_INET;
    addr_in.sin_port        = htons((uint16_t)port);
    addr_in.sin_addr.s_addr = addr;

    set_status("connecting");
    render_window();

    if (connect(sockfd, (struct sockaddr *)&addr_in, sizeof(addr_in)) < 0) {
        append_line("connect() failed", HTTPGET_COLOR_ERROR);
        set_status("connect failed");
        close(sockfd);
        return;
    }

    append_line("Connected.", HTTPGET_COLOR_INFO);

    char request[2048];
    int  body_len = body ? (int)strlen(body) : 0;
    int  req_len  = 0;

    if (body_len > 0) {
        req_len = snprintf(request, sizeof(request),
                           "%s %s HTTP/1.0\r\n"
                           "Host: %s\r\n"
                           "Connection: close\r\n"
                           "Content-Length: %d\r\n"
                           "\r\n"
                           "%s",
                           method, path, host, body_len, body);
    } else {
        req_len = snprintf(request, sizeof(request),
                           "%s %s HTTP/1.0\r\n"
                           "Host: %s\r\n"
                           "Connection: close\r\n"
                           "\r\n",
                           method, path, host);
    }

    if (req_len <= 0 || req_len >= (int)sizeof(request)) {
        append_line("Request is too large.", HTTPGET_COLOR_ERROR);
        set_status("request too large");
        close(sockfd);
        return;
    }

    if (socket_send_all(sockfd, request, req_len) < 0) {
        append_line("send() failed", HTTPGET_COLOR_ERROR);
        set_status("send failed");
        close(sockfd);
        return;
    }

    append_line("Request sent.", HTTPGET_COLOR_INFO);
    append_line("---------------- RESPONSE ----------------", HTTPGET_COLOR_INFO);
    set_status("reading response");
    render_window();

    char buffer[512];
    while (true) {
        int got = read(sockfd, buffer, sizeof(buffer));
        if (got <= 0) {
            break;
        }
        append_response_chunk(buffer, got);
        render_window();
    }

    append_line("-------------- END RESPONSE --------------", HTTPGET_COLOR_INFO);
    set_status("done");
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
}

static void httpget_message_proc(UINT64 type, UINT64 hData, UINT64 lData)
{
    (void)hData;

    switch (type) {
    case MSG_CHAR:
        if ((char)lData >= ' ' && g_input_len < (int)sizeof(g_input) - 1) {
            g_input[g_input_len++] = (char)lData;
            g_input[g_input_len]   = '\0';
            g_need_redraw          = true;
        }
        break;
    case MSG_SPCHAR:
        if ((char)lData == '\b') {
            if (g_input_len > 0) {
                g_input[--g_input_len] = '\0';
                g_need_redraw          = true;
            }
        } else if ((char)lData == '\n') {
            g_send_req = true;
        }
        break;
    default:
        break;
    }
}

static int httpget_main_impl(int argc, char *argv[], char *envp[])
{
    (void)argc;
    (void)argv;
    (void)envp;

    XWINDOW window;
    window.title  = (char *)"HTTP Client";
    window.width  = HTTPGET_WIDTH;
    window.height = HTTPGET_HEIGHT;
    window.sets   = XWIN_NORMAL;

    xapi_CreateWindow(&g_window, &window);
    xapi_SetIcon(g_window, "/system/icon/terminal.png");
    SetMsgPrcor(g_window, httpget_message_proc);

    append_line("Ready to send HTTP requests.", HTTPGET_COLOR_INFO);
    render_window();

    while (true) {
        if (g_send_req) {
            run_http_request();
        }
        if (g_need_redraw) {
            render_window();
        }
        xapi_Sleep(50);
    }

    return 0;
}

extern "C" int httpget_main_cpp(int argc, char *argv[], char *envp[]) asm("_Z4mainiPPcS0_");
extern "C" int httpget_main_cpp(int argc, char *argv[], char *envp[])
{
    return httpget_main_impl(argc, argv, envp);
}
