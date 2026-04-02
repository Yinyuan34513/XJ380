#include "./xapi/include/x3api.h"

static const uint64_t CHAT_POLL_INTERVAL_MS = 500;
static const uint64_t CHAT_LOOP_SLEEP_MS    = 50;
static const int      CHAT_WIDTH            = 720;
static const int      CHAT_HEIGHT           = 480;
static const int      CHAT_MAX_LINES        = 128;
static const int      CHAT_LINE_LENGTH      = 256;
static const int      CHAT_VISIBLE_LINES    = 22;

typedef struct chat_line {
    char     text[CHAT_LINE_LENGTH];
    uint32_t color;
} chat_line_t;

static HDLE        g_window              = 0;
static int         g_listen_fd           = -1;
static int         g_sockfd              = -1;
static uint64_t    g_poll_elapsed_ms     = 0;
static bool        g_need_redraw         = true;
static bool        g_send_requested      = false;
static int         g_input_len           = 0;
static char        g_input[512]          = {0};
static char        g_status[128]         = "Starting chat...";
static chat_line_t g_lines[CHAT_MAX_LINES];
static int         g_line_count          = 0;

static void set_status(const char *text)
{
    strncpy(g_status, text, sizeof(g_status) - 1);
    g_status[sizeof(g_status) - 1] = '\0';
    g_need_redraw                  = true;
}

static void append_line(const char *text, uint32_t color)
{
    if (text == NULL || text[0] == '\0') {
        return;
    }

    if (g_line_count == CHAT_MAX_LINES) {
        for (int i = 1; i < CHAT_MAX_LINES; ++i) {
            g_lines[i - 1] = g_lines[i];
        }
        g_line_count--;
    }

    strncpy(g_lines[g_line_count].text, text, CHAT_LINE_LENGTH - 1);
    g_lines[g_line_count].text[CHAT_LINE_LENGTH - 1] = '\0';
    g_lines[g_line_count].color                      = color;
    g_line_count++;
    g_need_redraw = true;
}

static void append_prefixed_line(const char *prefix, const char *data, int len, uint32_t color)
{
    char line[CHAT_LINE_LENGTH];
    int  prefix_len = (int)strlen(prefix);
    int  copy_len   = len;

    if (copy_len < 0) {
        copy_len = 0;
    }
    if (prefix_len + copy_len + 1 > CHAT_LINE_LENGTH) {
        copy_len = CHAT_LINE_LENGTH - prefix_len - 1;
    }

    memcpy(line, prefix, (uint64_t)prefix_len);
    if (copy_len > 0) {
        memcpy(line + prefix_len, data, (uint64_t)copy_len);
    }
    line[prefix_len + copy_len] = '\0';

    // Drop a trailing newline so the renderer keeps one message per row.
    int out_len = prefix_len + copy_len;
    while (out_len > 0 && (line[out_len - 1] == '\n' || line[out_len - 1] == '\r')) {
        line[--out_len] = '\0';
    }

    append_line(line, color);
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

static void render_chat_window()
{
    xapi_DrawRect(g_window, 0, 0, CHAT_WIDTH - 1, CHAT_HEIGHT - 1, 0xf4efe6ff, true);
    xapi_DrawRect(g_window, 0, 0, CHAT_WIDTH - 1, 39, 0x264653ff, true);
    xapi_DrawRect(g_window, 0, CHAT_HEIGHT - 72, CHAT_WIDTH - 1, CHAT_HEIGHT - 1, 0xe9dcc9ff, true);
    xapi_DrawRect(g_window, 12, 52, CHAT_WIDTH - 13, CHAT_HEIGHT - 84, 0xfffbf5ff, true);
    xapi_DrawRect(g_window, 12, CHAT_HEIGHT - 60, CHAT_WIDTH - 13, CHAT_HEIGHT - 16, 0xffffffff, true);

    xapi_DrawSWText(g_window, 16, 12, (char *)"XJ380 Chat", 0xffffffff);
    xapi_DrawSWText(g_window, 190, 12, g_status, 0xd8f3dcff);
    xapi_DrawSWText(g_window, 16, CHAT_HEIGHT - 54, (char *)"Input:", 0x3d405bff);
    xapi_DrawSWText(g_window, 72, CHAT_HEIGHT - 54, g_input, 0x111111ff);

    int start = g_line_count > CHAT_VISIBLE_LINES ? g_line_count - CHAT_VISIBLE_LINES : 0;
    for (int i = start; i < g_line_count; ++i) {
        xapi_DrawSWText(g_window, 20, 60 + (i - start) * 16, g_lines[i].text, g_lines[i].color);
    }

    xapi_RefreshWindow(g_window);
    g_need_redraw = false;
}

static bool setup_listener()
{
    g_listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (g_listen_fd < 0) {
        append_line("chat: socket() failed", 0xb00020ff);
        set_status("socket failed");
        return false;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_port        = htons(2323);
    addr.sin_addr.s_addr = 0;

    if (bind(g_listen_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        append_line("chat: bind(0.0.0.0:2323) failed", 0xb00020ff);
        set_status("bind failed");
        close(g_listen_fd);
        g_listen_fd = -1;
        return false;
    }

    if (listen(g_listen_fd, 1) < 0) {
        append_line("chat: listen() failed", 0xb00020ff);
        set_status("listen failed");
        close(g_listen_fd);
        g_listen_fd = -1;
        return false;
    }

    append_line("Listening on 0.0.0.0:2323", 0x1d3557ff);
    append_line("Host can connect to 127.0.0.1:2323", 0x1d3557ff);
    set_status("waiting for host");
    return true;
}

static void poll_listener_once()
{
    if (g_listen_fd < 0 || g_sockfd >= 0) {
        return;
    }

    struct pollfd fd;
    fd.fd      = g_listen_fd;
    fd.events  = POLLIN;
    fd.revents = 0;

    int ready = poll(&fd, 1, 0);
    if (ready <= 0) {
        return;
    }
    if (!(fd.revents & POLLIN)) {
        return;
    }

    struct sockaddr_in peer_addr;
    socklen_t          peer_len = sizeof(peer_addr);
    g_sockfd                    = accept(g_listen_fd, (struct sockaddr *)&peer_addr, &peer_len);
    if (g_sockfd < 0) {
        append_line("chat: accept() failed", 0xb00020ff);
        set_status("accept failed");
        return;
    }

    close(g_listen_fd);
    g_listen_fd = -1;
    append_line("Host connected.", 0x2a9d8fff);
    set_status("connected");
}

static void poll_socket_once()
{
    if (g_sockfd < 0) {
        return;
    }

    struct pollfd fd;
    fd.fd      = g_sockfd;
    fd.events  = POLLIN;
    fd.revents = 0;

    int ready = poll(&fd, 1, 0);
    if (ready <= 0) {
        return;
    }

    if (fd.revents & (POLLHUP | POLLERR | POLLNVAL)) {
        append_line("Peer disconnected.", 0xb00020ff);
        set_status("disconnected");
        close(g_sockfd);
        g_sockfd = -1;
        return;
    }

    if (!(fd.revents & POLLIN)) {
        return;
    }

    char buffer[512];
    int  got = read(g_sockfd, buffer, sizeof(buffer));
    if (got <= 0) {
        append_line("Peer disconnected.", 0xb00020ff);
        set_status("disconnected");
        close(g_sockfd);
        g_sockfd = -1;
        return;
    }

    append_prefixed_line("[host] ", buffer, got, 0x264653ff);
}

static void send_input_line()
{
    if (!g_send_requested) {
        return;
    }

    g_send_requested = false;
    if (g_input_len == 0) {
        g_need_redraw = true;
        return;
    }

    if (g_sockfd < 0) {
        append_line("No host connected yet.", 0xb00020ff);
        g_input[0]   = '\0';
        g_input_len  = 0;
        g_need_redraw = true;
        return;
    }

    char output[sizeof(g_input) + 2];
    memcpy(output, g_input, (uint64_t)g_input_len);
    int len = g_input_len;
    output[len++] = '\n';
    output[len]   = '\0';

    if (socket_send_all(g_sockfd, output, len) < 0) {
        append_line("chat: send failed", 0xb00020ff);
        set_status("send failed");
        close(g_sockfd);
        g_sockfd = -1;
    } else {
        append_prefixed_line("[me] ", g_input, g_input_len, 0xe76f51ff);
    }

    g_input[0]    = '\0';
    g_input_len   = 0;
    g_need_redraw = true;
}

static void chat_message_proc(UINT64 type, UINT64 hData, UINT64 lData)
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
            g_send_requested = true;
        }
        break;
    default:
        break;
    }
}

static int chat_main_impl(int argc, char *argv[], char *envp[])
{
    (void)argc;
    (void)argv;
    (void)envp;

    XWINDOW window;
    window.title  = (char *)"Chat";
    window.width  = CHAT_WIDTH;
    window.height = CHAT_HEIGHT;
    window.sets   = XWIN_NORMAL;

    xapi_CreateWindow(&g_window, &window);
    xapi_SetIcon(g_window, "/system/icon/terminal.png");
    SetMsgPrcor(g_window, chat_message_proc);

    append_line("Type a message and press Enter to send.", 0x3d405bff);
    append_line("Socket is polled every 0.5 seconds.", 0x3d405bff);
    setup_listener();
    render_chat_window();

    while (true) {
        send_input_line();

        g_poll_elapsed_ms += CHAT_LOOP_SLEEP_MS;
        if (g_poll_elapsed_ms >= CHAT_POLL_INTERVAL_MS) {
            g_poll_elapsed_ms = 0;
            poll_listener_once();
            poll_socket_once();
        }

        if (g_need_redraw) {
            render_chat_window();
        }

        xapi_Sleep(CHAT_LOOP_SLEEP_MS);
    }

    return 0;
}

extern "C" int chat_main_cpp(int argc, char *argv[], char *envp[]) asm("_Z4mainiPPcS0_");
extern "C" int chat_main_cpp(int argc, char *argv[], char *envp[])
{
    return chat_main_impl(argc, argv, envp);
}
