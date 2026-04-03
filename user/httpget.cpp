#include "httpclient.h"

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

static void render_window();

typedef struct httpget_line {
    char     text[HTTPGET_LINE_LENGTH];
    uint32_t color;
} httpget_line_t;

typedef struct httpget_response_state {
    char line[HTTPGET_LINE_LENGTH];
    int  line_len;
} httpget_response_state_t;

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

static void append_response_line(httpget_response_state_t *state)
{
    state->line[state->line_len] = '\0';
    append_line(state->line_len == 0 ? "" : state->line, HTTPGET_COLOR_RESPONSE);
    state->line_len = 0;
}

static int httpget_response_callback(const char *data, int len, void *user_data)
{
    httpget_response_state_t *state = (httpget_response_state_t *)user_data;
    for (int i = 0; i < len; ++i) {
        char ch = data[i];
        if (ch == '\r') {
            continue;
        }
        if (ch == '\n' || state->line_len >= HTTPGET_LINE_LENGTH - 1) {
            append_response_line(state);
            if (ch == '\n') {
                continue;
            }
        }
        state->line[state->line_len++] = ch;
    }

    render_window();
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
    xapi_DrawSWText(g_window, 18, 72, (char *)"Methods: GET POST PUT PATCH DELETE HEAD OPTIONS TRACE CONNECT", HTTPGET_COLOR_INFO);
    xapi_DrawSWText(g_window, 18, 88, (char *)"Example: POST 10.0.2.2 80 /api hello world", HTTPGET_COLOR_INFO);
    xapi_DrawSWText(g_window, 18, HTTPGET_HEIGHT - 56, (char *)"Request:", HTTPGET_COLOR_INFO);
    xapi_DrawSWText(g_window, 90, HTTPGET_HEIGHT - 56, g_input, HTTPGET_COLOR_TEXT);

    int start = g_line_count > HTTPGET_VISIBLE_LINES ? g_line_count - HTTPGET_VISIBLE_LINES : 0;
    for (int i = start; i < g_line_count; ++i) {
        xapi_DrawSWText(g_window, 20, 120 + (i - start) * 16, g_lines[i].text, g_lines[i].color);
    }

    xapi_RefreshWindow(g_window);
    g_need_redraw = false;
}

static void run_http_request()
{
    g_send_req = false;
    xapi_OutputSerial((char *)"httpget: run_http_request\n");

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

    if (!xhttp_validate_method(method)) {
        append_line("Invalid HTTP method token.", HTTPGET_COLOR_ERROR);
        set_status("bad method");
        return;
    }

    uint32_t parsed_addr = 0;
    if (!xhttp_parse_ipv4(host, &parsed_addr)) {
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

    xhttp_request_t request;
    request.method        = method;
    request.host          = host;
    request.port          = (uint16_t)port;
    request.path          = path;
    request.body          = body;
    request.content_type  = "text/plain";
    request.extra_headers = "";

    httpget_response_state_t response_state;
    memset(&response_state, 0, sizeof(response_state));

    append_line("Request queued.", HTTPGET_COLOR_INFO);
    append_line("---------------- RESPONSE ----------------", HTTPGET_COLOR_INFO);
    set_status("requesting");
    render_window();

    int status = xhttp_request(&request, httpget_response_callback, &response_state);
    xapi_Printf((char *)"httpget: xhttp_request status=%d\n", status);
    if (response_state.line_len > 0) {
        append_response_line(&response_state);
    }

    if (status == XHTTP_OK) {
        append_line("-------------- END RESPONSE --------------", HTTPGET_COLOR_INFO);
        set_status("done");
        return;
    }

    switch (status) {
    case XHTTP_ERR_SOCKET:
        append_line("socket() failed", HTTPGET_COLOR_ERROR);
        set_status("socket failed");
        break;
    case XHTTP_ERR_CONNECT:
        append_line("connect() failed", HTTPGET_COLOR_ERROR);
        set_status("connect failed");
        break;
    case XHTTP_ERR_REQUEST_BUILD:
        append_line("Request is too large.", HTTPGET_COLOR_ERROR);
        set_status("request too large");
        break;
    case XHTTP_ERR_SEND:
        append_line("send() failed", HTTPGET_COLOR_ERROR);
        set_status("send failed");
        break;
    case XHTTP_ERR_RECV:
        append_line("read() failed", HTTPGET_COLOR_ERROR);
        set_status("recv failed");
        break;
    case XHTTP_ERR_TIMEOUT:
        append_line("request timed out", HTTPGET_COLOR_ERROR);
        set_status("timeout");
        break;
    default:
        append_line("HTTP request failed.", HTTPGET_COLOR_ERROR);
        set_status("request failed");
        break;
    }

    append_line("-------------- END RESPONSE --------------", HTTPGET_COLOR_INFO);
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
