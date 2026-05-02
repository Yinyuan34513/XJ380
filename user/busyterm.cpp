#include "xapi/include/x3api.h"

#define BUSY_WIDTH       720
#define BUSY_HEIGHT      405
#define TERM_LEFT_X      8
#define TERM_TOP_Y       4
#define TERM_RIGHT_X     (BUSY_WIDTH - 8)
#define TERM_BOTTOM_Y    (BUSY_HEIGHT - 16)
#define CELL_W           9
#define CELL_H           16
#define TERM_COLS        ((TERM_RIGHT_X - TERM_LEFT_X) / CELL_W)
#define TERM_ROWS        ((TERM_BOTTOM_Y - TERM_TOP_Y) / CELL_H)
#define COLOR_BG         0xffffffff
#define COLOR_FG         0x000000ff
#define SEQ_MAX          128
#define PARAM_MAX        16
#define PTY_CHUNK        1024

#define TIOCSCTTY        0x540E

enum TerminalKeyCode
{
    TERM_KEY_ESC = 128,
    TERM_KEY_TAB = 130,
    TERM_KEY_F1 = 136,
    TERM_KEY_F2,
    TERM_KEY_F3,
    TERM_KEY_F4,
    TERM_KEY_F5,
    TERM_KEY_F6,
    TERM_KEY_F7,
    TERM_KEY_F8,
    TERM_KEY_F9,
    TERM_KEY_F10,
    TERM_KEY_F11,
    TERM_KEY_F12,
    TERM_KEY_NUM_LOCK,
    TERM_KEY_SCROLL_LOCK,
    TERM_KEY_HOME,
    TERM_KEY_UP,
    TERM_KEY_PAGE_UP,
    TERM_KEY_LEFT,
    TERM_KEY_RIGHT,
    TERM_KEY_END,
    TERM_KEY_DOWN,
    TERM_KEY_PAGE_DOWN,
    TERM_KEY_INSERT,
    TERM_KEY_DELETE,
};

static HDLE g_window;
static int  g_child_pid = -1;
static int  g_pty_master = -1;
static int  g_spawn_error = 0;
static const char *g_spawn_stage = "start";
static int  g_cur_x = TERM_LEFT_X;
static int  g_cur_y = TERM_TOP_Y;
static int  g_saved_x = TERM_LEFT_X;
static int  g_saved_y = TERM_TOP_Y;
static int  g_parser_state = 0;
static char g_seq[SEQ_MAX];
static int  g_seq_len = 0;
static UINT32 g_fg = COLOR_FG;
static UINT32 g_bg = COLOR_BG;
static XCOLOR g_pixel_buffer[BUSY_WIDTH * BUSY_HEIGHT];

static UINT32 g_palette[16] = {
    0x000000ff, 0xaa0000ff, 0x00aa00ff, 0xaa5500ff,
    0x0000aaff, 0xaa00aaff, 0x00aaaaff, 0xaaaaaaff,
    0x555555ff, 0xff5555ff, 0x55ff55ff, 0xffff55ff,
    0x5555ffff, 0xff55ffff, 0x55ffffff, 0xffffffff,
};

static int clamp_i(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static int row()
{
    return clamp_i((g_cur_y - TERM_TOP_Y) / CELL_H, 0, TERM_ROWS - 1);
}

static int col()
{
    return clamp_i((g_cur_x - TERM_LEFT_X) / CELL_W, 0, TERM_COLS - 1);
}

static void set_cursor(int r, int c)
{
    r = clamp_i(r, 0, TERM_ROWS - 1);
    c = clamp_i(c, 0, TERM_COLS - 1);
    g_cur_x = TERM_LEFT_X + c * CELL_W;
    g_cur_y = TERM_TOP_Y + r * CELL_H;
}

static void fill_rect(int x1, int y1, int x2, int y2, UINT32 color)
{
    x1 = clamp_i(x1, 0, BUSY_WIDTH - 1);
    y1 = clamp_i(y1, 0, BUSY_HEIGHT - 1);
    x2 = clamp_i(x2, 0, BUSY_WIDTH - 1);
    y2 = clamp_i(y2, 0, BUSY_HEIGHT - 1);
    if (x2 < x1 || y2 < y1) return;
    xapi_DrawRect(g_window, x1, y1, x2, y2, color, true);
}

static void clear_screen()
{
    xapi_DrawRect(g_window, 0, 0, BUSY_WIDTH - 1, BUSY_HEIGHT - 1, COLOR_BG, true);
    set_cursor(0, 0);
}

static void scroll_up()
{
    const int y = TERM_TOP_Y;
    const int h = (TERM_ROWS - 1) * CELL_H;
    xapi_ReadBuffer(g_window, 0, y + CELL_H, BUSY_WIDTH, h, g_pixel_buffer);
    xapi_WriteBuffer(g_window, 0, y, BUSY_WIDTH, h, g_pixel_buffer);
    fill_rect(0, y + h, BUSY_WIDTH - 1, y + h + CELL_H - 1, g_bg);
}

static void newline()
{
    int r = row();
    if (r >= TERM_ROWS - 1) scroll_up();
    else r++;
    set_cursor(r, 0);
}

static int utf8_len(unsigned char c)
{
    if ((c & 0x80) == 0x00) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

static void draw_char(const char *text, int len, int cells)
{
    if (col() + cells > TERM_COLS) newline();
    int w = cells * CELL_W;
    fill_rect(g_cur_x, g_cur_y, g_cur_x + w - 1, g_cur_y + CELL_H - 1, g_bg);
    char tmp[5] = {0};
    for (int i = 0; i < len && i < 4; i++) tmp[i] = text[i];
    xapi_DrawSWText(g_window, g_cur_x, g_cur_y, tmp, g_fg);
    if (col() + cells >= TERM_COLS) newline();
    else set_cursor(row(), col() + cells);
}

static void erase_line(int mode)
{
    int y = TERM_TOP_Y + row() * CELL_H;
    if (mode == 0) fill_rect(g_cur_x, y, TERM_RIGHT_X - 1, y + CELL_H - 1, g_bg);
    else if (mode == 1) fill_rect(TERM_LEFT_X, y, g_cur_x + CELL_W - 1, y + CELL_H - 1, g_bg);
    else fill_rect(TERM_LEFT_X, y, TERM_RIGHT_X - 1, y + CELL_H - 1, g_bg);
}

static void erase_display(int mode)
{
    int r = row();
    int y = TERM_TOP_Y + r * CELL_H;
    if (mode == 0)
    {
        erase_line(0);
        fill_rect(TERM_LEFT_X, y + CELL_H, TERM_RIGHT_X - 1, TERM_TOP_Y + TERM_ROWS * CELL_H - 1, g_bg);
    }
    else if (mode == 1)
    {
        fill_rect(TERM_LEFT_X, TERM_TOP_Y, TERM_RIGHT_X - 1, y - 1, g_bg);
        erase_line(1);
    }
    else clear_screen();
}

static void parse_params(int *params, int *count)
{
    *count = 0;
    int value = 0;
    bool have = false;
    int i = 0;
    if (i < g_seq_len && (g_seq[i] == '?' || g_seq[i] == '>')) i++;
    for (; i <= g_seq_len && *count < PARAM_MAX; i++)
    {
        char c = i < g_seq_len ? g_seq[i] : ';';
        if (c >= '0' && c <= '9')
        {
            value = value * 10 + (c - '0');
            have = true;
        }
        else if (c == ';' || c == ':')
        {
            params[(*count)++] = have ? value : -1;
            value = 0;
            have = false;
        }
    }
}

static int param_or(int *params, int count, int idx, int defv)
{
    if (idx >= count || params[idx] < 0) return defv;
    return params[idx];
}

static void apply_sgr(int *params, int count)
{
    if (count == 0)
    {
        g_fg = COLOR_FG;
        g_bg = COLOR_BG;
        return;
    }
    bool bold = false;
    for (int i = 0; i < count; i++)
    {
        int p = params[i] < 0 ? 0 : params[i];
        if (p == 0)
        {
            g_fg = COLOR_FG;
            g_bg = COLOR_BG;
            bold = false;
        }
        else if (p == 1) bold = true;
        else if (p == 22) bold = false;
        else if (p == 39) g_fg = COLOR_FG;
        else if (p == 49) g_bg = COLOR_BG;
        else if (p >= 30 && p <= 37) g_fg = g_palette[p - 30 + (bold ? 8 : 0)];
        else if (p >= 40 && p <= 47) g_bg = g_palette[p - 40];
        else if (p >= 90 && p <= 97) g_fg = g_palette[p - 90 + 8];
        else if (p >= 100 && p <= 107) g_bg = g_palette[p - 100 + 8];
    }
}

static void dispatch_csi(char final_char)
{
    int params[PARAM_MAX];
    int count = 0;
    parse_params(params, &count);
    int n = param_or(params, count, 0, 1);

    switch (final_char)
    {
    case 'A': set_cursor(row() - n, col()); break;
    case 'B': set_cursor(row() + n, col()); break;
    case 'C': set_cursor(row(), col() + n); break;
    case 'D': set_cursor(row(), col() - n); break;
    case 'H':
    case 'f': set_cursor(param_or(params, count, 0, 1) - 1, param_or(params, count, 1, 1) - 1); break;
    case 'G': set_cursor(row(), param_or(params, count, 0, 1) - 1); break;
    case 'J': erase_display(param_or(params, count, 0, 0)); break;
    case 'K': erase_line(param_or(params, count, 0, 0)); break;
    case 'm': apply_sgr(params, count); break;
    case 's': g_saved_x = g_cur_x; g_saved_y = g_cur_y; break;
    case 'u': g_cur_x = g_saved_x; g_cur_y = g_saved_y; break;
    default: break;
    }
}

static void append_seq(unsigned char c)
{
    if (g_seq_len < SEQ_MAX - 1) g_seq[g_seq_len++] = (char)c;
}

static void process_byte(unsigned char c)
{
    if (g_parser_state == 1)
    {
        if (c == '[')
        {
            g_parser_state = 2;
            g_seq_len = 0;
        }
        else if (c == '7')
        {
            g_saved_x = g_cur_x;
            g_saved_y = g_cur_y;
            g_parser_state = 0;
        }
        else if (c == '8')
        {
            g_cur_x = g_saved_x;
            g_cur_y = g_saved_y;
            g_parser_state = 0;
        }
        else if (c == 'c')
        {
            clear_screen();
            g_parser_state = 0;
        }
        else g_parser_state = 0;
        return;
    }
    if (g_parser_state == 2)
    {
        if (c >= 0x40 && c <= 0x7e)
        {
            dispatch_csi((char)c);
            g_parser_state = 0;
        }
        else append_seq(c);
        return;
    }

    if (c == '\x1b') { g_parser_state = 1; return; }
    if (c == '\n') { newline(); return; }
    if (c == '\r') { set_cursor(row(), 0); return; }
    if (c == '\b') { if (col() > 0) set_cursor(row(), col() - 1); return; }
    if (c == '\t') { set_cursor(row(), ((col() / 8) + 1) * 8); return; }
    if (c < 0x20 || c == 0x7f) return;
}

static void terminal_write(const char *buf, int len)
{
    for (int i = 0; i < len;)
    {
        unsigned char c = (unsigned char)buf[i];
        if (g_parser_state != 0 || c == '\x1b' || c < 0x20 || c == 0x7f)
        {
            process_byte(c);
            i++;
            continue;
        }
        int clen = utf8_len(c);
        if (i + clen > len) clen = 1;
        draw_char(&buf[i], clen, clen == 1 ? 1 : 2);
        i += clen;
    }
}

static void send_child(const char *buf, int len)
{
    if (buf == NULL || len <= 0 || g_pty_master < 0) return;
    write(g_pty_master, buf, (size_t)len);
}

static void send_key_seq(uint64_t key)
{
    const char *seq = NULL;
    switch (key)
    {
    case TERM_KEY_ESC: seq = "\x1b"; break;
    case TERM_KEY_TAB: seq = "\t"; break;
    case TERM_KEY_F1: seq = "\x1bOP"; break;
    case TERM_KEY_F2: seq = "\x1bOQ"; break;
    case TERM_KEY_F3: seq = "\x1bOR"; break;
    case TERM_KEY_F4: seq = "\x1bOS"; break;
    case TERM_KEY_F5: seq = "\x1b[15~"; break;
    case TERM_KEY_F6: seq = "\x1b[17~"; break;
    case TERM_KEY_F7: seq = "\x1b[18~"; break;
    case TERM_KEY_F8: seq = "\x1b[19~"; break;
    case TERM_KEY_F9: seq = "\x1b[20~"; break;
    case TERM_KEY_F10: seq = "\x1b[21~"; break;
    case TERM_KEY_F11: seq = "\x1b[23~"; break;
    case TERM_KEY_F12: seq = "\x1b[24~"; break;
    case TERM_KEY_HOME: seq = "\x1b[H"; break;
    case TERM_KEY_UP: seq = "\x1b[A"; break;
    case TERM_KEY_PAGE_UP: seq = "\x1b[5~"; break;
    case TERM_KEY_LEFT: seq = "\x1b[D"; break;
    case TERM_KEY_RIGHT: seq = "\x1b[C"; break;
    case TERM_KEY_END: seq = "\x1b[F"; break;
    case TERM_KEY_DOWN: seq = "\x1b[B"; break;
    case TERM_KEY_PAGE_DOWN: seq = "\x1b[6~"; break;
    case TERM_KEY_INSERT: seq = "\x1b[2~"; break;
    case TERM_KEY_DELETE: seq = "\x1b[3~"; break;
    default: return;
    }
    send_child(seq, (int)strlen(seq));
}

static void busyterm_msg(UINT64 type, UINT64 hData, UINT64 lData)
{
    (void)hData;
    if (type == MSG_CHAR)
    {
        char c = (char)lData;
        // terminal_write(&c, 1);
        xapi_RefreshWindow(g_window);
        send_child(&c, 1);
    }
    else if (type == MSG_SPCHAR)
    {
        if (lData == '\n')
        {
            char c = '\n';
            terminal_write(&c, 1);
            xapi_RefreshWindow(g_window);
            send_child(&c, 1);
        }
        else if (lData == '\b')
        {
            char c = 0x7f;
            send_child(&c, 1);
        }
        else send_key_seq(lData);
    }
}

static int spawn_busybox(char *envp[])
{
    g_spawn_error = 0;
    g_spawn_stage = "posix_openpt";
    g_pty_master = posix_openpt(O_RDWR | O_NOCTTY);
    if (g_pty_master < 0)
    {
        g_spawn_error = errno;
        return -1;
    }
    g_spawn_stage = "grantpt";
    if (grantpt(g_pty_master) < 0)
    {
        g_spawn_error = errno;
        close(g_pty_master);
        g_pty_master = -1;
        return -1;
    }
    g_spawn_stage = "unlockpt";
    if (unlockpt(g_pty_master) < 0)
    {
        g_spawn_error = errno;
        close(g_pty_master);
        g_pty_master = -1;
        return -1;
    }

    g_spawn_stage = "ptsname";
    char *slave_name = ptsname(g_pty_master);
    if (slave_name == NULL)
    {
        g_spawn_error = errno;
        close(g_pty_master);
        g_pty_master = -1;
        return -1;
    }

    g_spawn_stage = "fork";
    int pid = fork();
    if (pid == 0)
    {
        int slave = open(slave_name, O_RDWR | O_NOCTTY);
        if (slave < 0) exit(126);

        ioctl(slave, TIOCSCTTY, 0);
        dup2(slave, 0);
        dup2(slave, 1);
        dup2(slave, 2);
        if (slave > 2) close(slave);
        close(g_pty_master);

        char *argv[] = {(char *)"/apps/busybox", (char *)"sh", (char *)"-i", NULL};
        execve("/apps/busybox", argv, envp);
        exit(127);
    }
    if (pid < 0)
    {
        g_spawn_error = errno;
        close(g_pty_master);
        g_pty_master = -1;
        return -1;
    }

    g_child_pid = pid;
    return pid;
}

static void drain_pty_output()
{
    if (g_pty_master < 0) return;

    struct pollfd pfd;
    pfd.fd = g_pty_master;
    pfd.events = POLLIN | POLLHUP;
    pfd.revents = 0;

    for (;;)
    {
        int ready = poll(&pfd, 1, 0);
        if (ready <= 0 || !(pfd.revents & (POLLIN | POLLHUP))) break;

        char output[PTY_CHUNK];
        ssize_t got = read(g_pty_master, output, sizeof(output));
        if (got > 0)
        {
            terminal_write(output, (int)got);
            xapi_RefreshWindow(g_window);
        }
        else
        {
            break;
        }

        if (pfd.revents & POLLHUP) break;
        pfd.revents = 0;
    }
}

static int busyterm_main(int argc, char *argv[], char *envp[])
{
    (void)argc;
    (void)argv;
    XWINDOW win;
    win.title = (char *)"BusyBox";
    win.width = BUSY_WIDTH;
    win.height = BUSY_HEIGHT;
    win.sets = XWIN_NORMAL;
    xapi_CreateWindow(&g_window, &win);
    xapi_SetIcon(g_window, "/system/icon/terminal.png");
    SetMsgPrcor(g_window, busyterm_msg);
    clear_screen();
    xapi_RefreshWindow(g_window);

    if (spawn_busybox(envp) < 0)
    {
        char msg[96];
        snprintf(msg, sizeof(msg), "busyterm: failed to start /apps/busybox sh at %s errno=%d\n",
                 g_spawn_stage, g_spawn_error);
        terminal_write(msg, (int)strlen(msg));
        xapi_RefreshWindow(g_window);
    }

    while (true)
    {
        drain_pty_output();

        if (g_child_pid > 0)
        {
            int status = 0;
            if (waitpid(g_child_pid, &status, WNOHANG) == g_child_pid)
            {
                g_child_pid = -1;
                const char *msg = "\n[busybox exited]\n";
                terminal_write(msg, (int)strlen(msg));
                xapi_RefreshWindow(g_window);
                if (g_pty_master >= 0)
                {
                    close(g_pty_master);
                    g_pty_master = -1;
                }
            }
        }
        __asm__ __volatile__("pause");
    }
}

extern "C" int busyterm_cpp_main(int argc, char *argv[], char *envp[]) asm("_Z4mainiPPcS0_");
extern "C" int busyterm_cpp_main(int argc, char *argv[], char *envp[])
{
    return busyterm_main(argc, argv, envp);
}
