#include "xapi/include/x3api.h"
#include "vterm.h"

#define BUSY_WIDTH       720
#define BUSY_HEIGHT      405
#define TERM_LEFT_X      8
#define TERM_TOP_Y       4
#define TERM_RIGHT_PAD   8
#define TERM_BOTTOM_PAD  16
#define CELL_W           9
#define CELL_H           16
#define COLOR_BG         0x0b0f14ff
#define COLOR_FG         0xe6edf3ff
#define PTY_CHUNK        1024
#define DEFAULT_USER     "GuoqiFish"
#define DEFAULT_HOME     "/users/GuoqiFish"
#define DEFAULT_SHELL    "/bin/sh"
#define BUSYBOX_PATH     "/apps/busybox"

#define TIOCSCTTY        0x540E
#ifndef TIOCSWINSZ
#define TIOCSWINSZ       0x5414
#endif

enum TerminalKeyCode
{
    TERM_KEY_ESC = 128,
    TERM_KEY_TAB = 130,
    TERM_KEY_SHIFT = 133,
    TERM_KEY_CTRL,
    TERM_KEY_ALT,
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

static constexpr uint64_t TERM_KEY_CTRL_RELEASE = 0x9d;

static HDLE g_window;
static int  g_child_pid = -1;
static int  g_pty_master = -1;
static int  g_spawn_error = 0;
static const char *g_spawn_stage = "start";
static bool g_ctrl_down = false;
static int  g_win_width = BUSY_WIDTH;
static int  g_win_height = BUSY_HEIGHT;
static int  g_term_cols = (BUSY_WIDTH - TERM_LEFT_X - TERM_RIGHT_PAD) / CELL_W;
static int  g_term_rows = (BUSY_HEIGHT - TERM_TOP_Y - TERM_BOTTOM_PAD) / CELL_H;
static VTerm *g_vterm = NULL;
static VTermScreen *g_vscreen = NULL;
static VTermPos g_cursor = {0, 0};
static bool g_cursor_visible = true;
static XCOLOR *g_scroll_buffer = NULL;
static size_t  g_scroll_buffer_pixels = 0;

static void send_child(const char *buf, int len);

static UINT32 g_palette[16] = {
    0x0b0f14ff, 0xff7b72ff, 0x3fb950ff, 0xd29922ff,
    0x58a6ffff, 0xbc8cff,   0x39c5cfff, 0xb1bac4ff,
    0x6e7681ff, 0xffa198ff, 0x56d364ff, 0xe3b341ff,
    0x79c0ffff, 0xd2a8ffff, 0x56d4ddff, 0xf0f6fcff,
};

static UINT32 rgb_color(int r, int g, int b)
{
    if (r < 0) r = 0;
    if (r > 255) r = 255;
    if (g < 0) g = 0;
    if (g > 255) g = 255;
    if (b < 0) b = 0;
    if (b > 255) b = 255;
    return ((UINT32)r << 24) | ((UINT32)g << 16) | ((UINT32)b << 8) | 0xffU;
}

static UINT32 xterm_256_color(int index)
{
    if (index < 0) index = 0;
    if (index > 255) index = 255;
    if (index < 16) return g_palette[index];

    if (index < 232)
    {
        static const int levels[6] = {0, 95, 135, 175, 215, 255};
        int n = index - 16;
        return rgb_color(levels[n / 36], levels[(n / 6) % 6], levels[n % 6]);
    }

    int gray = 8 + (index - 232) * 10;
    return rgb_color(gray, gray, gray);
}

static int env_count(char *envp[])
{
    int count = 0;
    if (envp == NULL) return 0;
    while (envp[count] != NULL) count++;
    return count;
}

static bool env_has_key(char *envp[], const char *key)
{
    if (envp == NULL || key == NULL) return false;
    size_t key_len = strlen(key);
    for (int i = 0; envp[i] != NULL; i++)
    {
        if (strncmp(envp[i], key, key_len) == 0 && envp[i][key_len] == '=') return true;
    }
    return false;
}

static char **build_shell_env(char *envp[])
{
    static const char *extra_env[] = {
        "TERM=xterm-256color",
        "HOME=" DEFAULT_HOME,
        "USER=" DEFAULT_USER,
        "LOGNAME=" DEFAULT_USER,
        "HOSTNAME=localhost",
        "PATH=/apps:/system:/bin:/usr/bin",
        "SHELL=" DEFAULT_SHELL,
        "PWD=/apps",
        "PS1=\\[\\033[01;32m\\]" DEFAULT_USER "@\\h\\[\\033[00m\\]:\\[\\033[01;34m\\]\\w\\[\\033[00m\\]\\\\$ ",
        NULL,
    };

    int base_count = env_count(envp);
    int extra_count = 0;
    for (int i = 0; extra_env[i] != NULL; i++)
    {
        const char *eq = strchr(extra_env[i], '=');
        if (eq == NULL) continue;
        char key[32];
        size_t key_len = (size_t)(eq - extra_env[i]);
        if (key_len >= sizeof(key)) key_len = sizeof(key) - 1;
        memcpy(key, extra_env[i], key_len);
        key[key_len] = '\0';
        if (!env_has_key(envp, key)) extra_count++;
    }

    char **child_env = (char **)calloc((size_t)(base_count + extra_count + 1), sizeof(char *));
    if (child_env == NULL) return envp;

    int out = 0;
    for (int i = 0; i < base_count; i++) child_env[out++] = envp[i];
    for (int i = 0; extra_env[i] != NULL; i++)
    {
        const char *eq = strchr(extra_env[i], '=');
        if (eq == NULL) continue;
        char key[32];
        size_t key_len = (size_t)(eq - extra_env[i]);
        if (key_len >= sizeof(key)) key_len = sizeof(key) - 1;
        memcpy(key, extra_env[i], key_len);
        key[key_len] = '\0';
        if (!env_has_key(envp, key)) child_env[out++] = (char *)extra_env[i];
    }
    child_env[out] = NULL;
    return child_env;
}

static int clamp_i(int v, int lo, int hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static bool ensure_scroll_buffer(int width, int height)
{
    if (width <= 0 || height <= 0) return false;
    size_t pixels = (size_t)width * (size_t)height;
    if (pixels <= g_scroll_buffer_pixels && g_scroll_buffer != NULL) return true;

    XCOLOR *buffer = (XCOLOR *)realloc(g_scroll_buffer, pixels * sizeof(XCOLOR));
    if (buffer == NULL) return false;
    g_scroll_buffer = buffer;
    g_scroll_buffer_pixels = pixels;
    return true;
}

static void calculate_terminal_geometry(int width, int height, int *cols, int *rows)
{
    int min_width = TERM_LEFT_X + TERM_RIGHT_PAD + CELL_W;
    int min_height = TERM_TOP_Y + TERM_BOTTOM_PAD + CELL_H;
    if (width < min_width) width = min_width;
    if (height < min_height) height = min_height;

    int new_cols = (width - TERM_LEFT_X - TERM_RIGHT_PAD) / CELL_W;
    int new_rows = (height - TERM_TOP_Y - TERM_BOTTOM_PAD) / CELL_H;
    if (new_cols < 1) new_cols = 1;
    if (new_rows < 1) new_rows = 1;

    if (cols != NULL) *cols = new_cols;
    if (rows != NULL) *rows = new_rows;
}

static void sync_pty_window_size()
{
    if (g_pty_master < 0) return;

    struct winsize ws;
    memset(&ws, 0, sizeof(ws));
    ws.ws_col = (unsigned short)g_term_cols;
    ws.ws_row = (unsigned short)g_term_rows;
    ws.ws_xpixel = (unsigned short)(g_term_cols * CELL_W);
    ws.ws_ypixel = (unsigned short)(g_term_rows * CELL_H);
    ioctl(g_pty_master, TIOCSWINSZ, &ws);
}

static void fill_rect(int x1, int y1, int x2, int y2, UINT32 color)
{
    x1 = clamp_i(x1, 0, g_win_width - 1);
    y1 = clamp_i(y1, 0, g_win_height - 1);
    x2 = clamp_i(x2, 0, g_win_width - 1);
    y2 = clamp_i(y2, 0, g_win_height - 1);
    if (x2 < x1 || y2 < y1) return;
    xapi_DrawRect(g_window, x1, y1, x2, y2, color, true);
}

static void clear_window()
{
    xapi_DrawRect(g_window, 0, 0, g_win_width - 1, g_win_height - 1, COLOR_BG, true);
}

static void send_child(const char *buf, int len)
{
    if (buf == NULL || len <= 0 || g_pty_master < 0) return;
    write(g_pty_master, buf, (size_t)len);
}

static void utf8_encode(uint32_t cp, char *out)
{
    if (cp == 0)
    {
        out[0] = '\0';
        return;
    }
    if (cp < 0x80)
    {
        out[0] = (char)cp;
        out[1] = '\0';
        return;
    }
    if (cp < 0x800)
    {
        out[0] = (char)(0xc0 | (cp >> 6));
        out[1] = (char)(0x80 | (cp & 0x3f));
        out[2] = '\0';
        return;
    }
    if (cp < 0x10000)
    {
        out[0] = (char)(0xe0 | (cp >> 12));
        out[1] = (char)(0x80 | ((cp >> 6) & 0x3f));
        out[2] = (char)(0x80 | (cp & 0x3f));
        out[3] = '\0';
        return;
    }
    out[0] = (char)(0xf0 | (cp >> 18));
    out[1] = (char)(0x80 | ((cp >> 12) & 0x3f));
    out[2] = (char)(0x80 | ((cp >> 6) & 0x3f));
    out[3] = (char)(0x80 | (cp & 0x3f));
    out[4] = '\0';
}

static UINT32 vterm_color_to_xcolor(VTermColor color, UINT32 fallback)
{
    if (VTERM_COLOR_IS_DEFAULT_FG(&color) || VTERM_COLOR_IS_DEFAULT_BG(&color)) return fallback;
    if (VTERM_COLOR_IS_INDEXED(&color)) return xterm_256_color(color.indexed.idx);
    if (VTERM_COLOR_IS_RGB(&color)) return rgb_color(color.rgb.red, color.rgb.green, color.rgb.blue);
    return fallback;
}

static void draw_vterm_cell(int row, int col)
{
    if (g_vscreen == NULL || row < 0 || row >= g_term_rows || col < 0 || col >= g_term_cols) return;

    VTermScreenCell cell;
    VTermPos pos = {row, col};
    if (!vterm_screen_get_cell(g_vscreen, pos, &cell)) return;

    UINT32 fg = vterm_color_to_xcolor(cell.fg, COLOR_FG);
    UINT32 bg = vterm_color_to_xcolor(cell.bg, COLOR_BG);
    if (cell.attrs.reverse)
    {
        UINT32 tmp = fg;
        fg = bg;
        bg = tmp;
    }
    if (cell.attrs.bold && VTERM_COLOR_IS_INDEXED(&cell.fg) && cell.fg.indexed.idx < 8)
        fg = xterm_256_color(cell.fg.indexed.idx + 8);

    int x = TERM_LEFT_X + col * CELL_W;
    int y = TERM_TOP_Y + row * CELL_H;
    int w = cell.width > 1 ? cell.width * CELL_W : CELL_W;
    fill_rect(x, y, x + w - 1, y + CELL_H - 1, bg);

    if (!cell.attrs.conceal && cell.chars[0] != 0)
    {
        char text[(VTERM_MAX_CHARS_PER_CELL * 4) + 1];
        int out = 0;
        for (int i = 0; i < VTERM_MAX_CHARS_PER_CELL && cell.chars[i] != 0; i++)
        {
            char one[5];
            utf8_encode(cell.chars[i], one);
            for (int j = 0; one[j] != '\0' && out < (int)sizeof(text) - 1; j++)
                text[out++] = one[j];
        }
        text[out] = '\0';
        if (out > 0) xapi_DrawSWText(g_window, x, y, text, fg);
    }

    if (g_cursor_visible && row == g_cursor.row && col == g_cursor.col)
    {
        fill_rect(x, y + CELL_H - 2, x + CELL_W - 1, y + CELL_H - 1, fg);
    }
}

static void redraw_vterm_rect(VTermRect rect)
{
    rect.start_row = clamp_i(rect.start_row, 0, g_term_rows);
    rect.end_row = clamp_i(rect.end_row, 0, g_term_rows);
    rect.start_col = clamp_i(rect.start_col, 0, g_term_cols);
    rect.end_col = clamp_i(rect.end_col, 0, g_term_cols);
    for (int r = rect.start_row; r < rect.end_row; r++)
        for (int c = rect.start_col; c < rect.end_col; c++)
            draw_vterm_cell(r, c);
}

static int vterm_damage(VTermRect rect, void *user)
{
    (void)user;
    redraw_vterm_rect(rect);
    return 1;
}

static int vterm_moverect(VTermRect dest, VTermRect src, void *user)
{
    (void)user;
    int src_x = TERM_LEFT_X + src.start_col * CELL_W;
    int src_y = TERM_TOP_Y + src.start_row * CELL_H;
    int dst_x = TERM_LEFT_X + dest.start_col * CELL_W;
    int dst_y = TERM_TOP_Y + dest.start_row * CELL_H;
    int width = (src.end_col - src.start_col) * CELL_W;
    int height = (src.end_row - src.start_row) * CELL_H;
    if (width <= 0 || height <= 0) return 0;
    if (src_x < 0 || src_y < 0 || src_x + width > g_win_width || src_y + height > g_win_height) return 0;
    if (dst_x < 0 || dst_y < 0 || dst_x + width > g_win_width || dst_y + height > g_win_height) return 0;
    if (!ensure_scroll_buffer(width, height)) return 0;

    xapi_ReadBuffer(g_window, src_x, src_y, width, height, g_scroll_buffer);
    xapi_WriteBuffer(g_window, dst_x, dst_y, width, height, g_scroll_buffer);

    if (dest.start_row < src.start_row)
    {
        VTermRect exposed = {
            .start_row = dest.end_row,
            .end_row = src.end_row,
            .start_col = dest.start_col,
            .end_col = dest.end_col,
        };
        redraw_vterm_rect(exposed);
    }
    else if (dest.start_row > src.start_row)
    {
        VTermRect exposed = {
            .start_row = src.start_row,
            .end_row = dest.start_row,
            .start_col = dest.start_col,
            .end_col = dest.end_col,
        };
        redraw_vterm_rect(exposed);
    }
    if (g_cursor_visible) draw_vterm_cell(g_cursor.row, g_cursor.col);
    return 1;
}

static int vterm_movecursor(VTermPos pos, VTermPos oldpos, int visible, void *user)
{
    (void)user;
    VTermPos previous = g_cursor;
    g_cursor = pos;
    g_cursor_visible = visible != 0;
    draw_vterm_cell(oldpos.row, oldpos.col);
    draw_vterm_cell(previous.row, previous.col);
    draw_vterm_cell(g_cursor.row, g_cursor.col);
    return 1;
}

static int vterm_settermprop(VTermProp prop, VTermValue *val, void *user)
{
    (void)user;
    if (prop == VTERM_PROP_CURSORVISIBLE && val != NULL)
    {
        g_cursor_visible = val->boolean != 0;
        draw_vterm_cell(g_cursor.row, g_cursor.col);
    }
    return 1;
}

static void vterm_output(const char *s, size_t len, void *user)
{
    (void)user;
    send_child(s, (int)len);
}

static void terminal_init()
{
    if (g_vterm != NULL) return;

    struct VTermBuilder builder;
    memset(&builder, 0, sizeof(builder));
    builder.rows = g_term_rows;
    builder.cols = g_term_cols;
    g_vterm = vterm_build(&builder);
    if (g_vterm == NULL) return;

    vterm_set_utf8(g_vterm, 1);
    vterm_output_set_callback(g_vterm, vterm_output, NULL);
    g_vscreen = vterm_obtain_screen(g_vterm);
    if (g_vscreen == NULL) return;

    VTermColor fg;
    VTermColor bg;
    vterm_color_rgb(&fg, 0xe6, 0xed, 0xf3);
    vterm_color_rgb(&bg, 0x0b, 0x0f, 0x14);
    vterm_screen_set_default_colors(g_vscreen, &fg, &bg);

    static const VTermScreenCallbacks callbacks = {
        .damage = vterm_damage,
        .moverect = vterm_moverect,
        .movecursor = vterm_movecursor,
        .settermprop = vterm_settermprop,
        .bell = NULL,
        .resize = NULL,
        .sb_pushline = NULL,
        .sb_popline = NULL,
        .sb_clear = NULL,
        .sb_pushline4 = NULL,
    };
    vterm_screen_set_callbacks(g_vscreen, &callbacks, NULL);
    vterm_screen_set_damage_merge(g_vscreen, VTERM_DAMAGE_SCROLL);
    vterm_screen_enable_altscreen(g_vscreen, 1);
    vterm_screen_reset(g_vscreen, 1);
}

static void terminal_resize(int width, int height)
{
    int new_cols = 0;
    int new_rows = 0;
    calculate_terminal_geometry(width, height, &new_cols, &new_rows);

    g_win_width = width;
    g_win_height = height;
    if (g_win_width < TERM_LEFT_X + TERM_RIGHT_PAD + CELL_W) g_win_width = TERM_LEFT_X + TERM_RIGHT_PAD + CELL_W;
    if (g_win_height < TERM_TOP_Y + TERM_BOTTOM_PAD + CELL_H) g_win_height = TERM_TOP_Y + TERM_BOTTOM_PAD + CELL_H;

    if (new_cols != g_term_cols || new_rows != g_term_rows)
    {
        g_term_cols = new_cols;
        g_term_rows = new_rows;
        if (g_vterm != NULL) vterm_set_size(g_vterm, g_term_rows, g_term_cols);
    }

    g_cursor.row = clamp_i(g_cursor.row, 0, g_term_rows - 1);
    g_cursor.col = clamp_i(g_cursor.col, 0, g_term_cols - 1);
    sync_pty_window_size();
    clear_window();
    if (g_vscreen != NULL)
    {
        VTermRect all = {
            .start_row = 0,
            .end_row = g_term_rows,
            .start_col = 0,
            .end_col = g_term_cols,
        };
        redraw_vterm_rect(all);
    }
}

static bool terminal_write_chunk(const char *buf, int len, bool flush_damage)
{
    if (buf == NULL || len <= 0) return false;
    if (g_vterm == NULL) terminal_init();
    if (g_vterm == NULL)
    {
        xapi_DrawSWText(g_window, TERM_LEFT_X, TERM_TOP_Y, (char *)"busyterm: libvterm init failed", COLOR_FG);
        return true;
    }
    vterm_input_write(g_vterm, buf, (size_t)len);
    if (flush_damage && g_vscreen != NULL) vterm_screen_flush_damage(g_vscreen);
    return true;
}

static void terminal_flush_damage()
{
    if (g_vscreen != NULL) vterm_screen_flush_damage(g_vscreen);
}

static void terminal_write(const char *buf, int len)
{
    terminal_write_chunk(buf, len, true);
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

static bool ctrl_char(char in, char *out)
{
    if (out == NULL) return false;

    if (in >= 'a' && in <= 'z')
    {
        *out = (char)(in - 'a' + 1);
        return true;
    }
    if (in >= 'A' && in <= 'Z')
    {
        *out = (char)(in - 'A' + 1);
        return true;
    }

    switch (in)
    {
    case ' ': *out = 0x00; return true;
    case '@': *out = 0x00; return true;
    case '[': *out = 0x1b; return true;
    case '\\': *out = 0x1c; return true;
    case ']': *out = 0x1d; return true;
    case '^': *out = 0x1e; return true;
    case '6': *out = 0x1e; return true;
    case '_': *out = 0x1f; return true;
    case '-': *out = 0x1f; return true;
    case '?': *out = 0x7f; return true;
    default: return false;
    }
}

static void busyterm_msg(UINT64 type, UINT64 hData, UINT64 lData)
{
    (void)hData;
    if (type == MSG_CHAR)
    {
        char c = (char)lData;
        if (g_ctrl_down)
        {
            char ctrl = 0;
            if (ctrl_char(c, &ctrl))
            {
                send_child(&ctrl, 1);
                g_ctrl_down = false;
                return;
            }
        }
        send_child(&c, 1);
    }
    else if (type == MSG_SPCHAR)
    {
        if (lData == TERM_KEY_CTRL)
        {
            g_ctrl_down = true;
            return;
        }
        if (lData == TERM_KEY_CTRL_RELEASE)
        {
            g_ctrl_down = false;
            return;
        }
        if (lData == TERM_KEY_SHIFT || lData == TERM_KEY_ALT) return;

        if (lData == '\n')
        {
            char c = '\n';
            send_child(&c, 1);
        }
        else if (lData == '\b')
        {
            char c = 0x7f;
            send_child(&c, 1);
        }
        else send_key_seq(lData);
    }
    else if (type == MSG_RESIZE)
    {
        terminal_resize((int)hData, (int)lData);
        xapi_RefreshWindow(g_window);
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
    sync_pty_window_size();

    int pid = fork();
    if (pid == 0)
    {
        char **child_env = build_shell_env(envp);
        int slave = open(slave_name, O_RDWR | O_NOCTTY);
        if (slave < 0) exit(126);

        ioctl(slave, TIOCSCTTY, 0);
        dup2(slave, 0);
        dup2(slave, 1);
        dup2(slave, 2);
        if (slave > 2) close(slave);
        close(g_pty_master);

        char *shell_argv[] = {(char *)"sh", (char *)"-i", NULL};
        execve(DEFAULT_SHELL, shell_argv, child_env);
        execve(BUSYBOX_PATH, shell_argv, child_env);
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

    bool drew = false;
    for (;;)
    {
        int ready = poll(&pfd, 1, 0);
        if (ready <= 0 || !(pfd.revents & (POLLIN | POLLHUP))) break;

        char output[PTY_CHUNK];
        ssize_t got = read(g_pty_master, output, sizeof(output));
        if (got > 0)
        {
            drew |= terminal_write_chunk(output, (int)got, false);
        }
        else
        {
            break;
        }

        if (pfd.revents & POLLHUP) break;
        pfd.revents = 0;
    }

    if (drew)
    {
        terminal_flush_damage();
        xapi_RefreshWindow(g_window);
    }
}

static int busyterm_main(int argc, char *argv[], char *envp[])
{
    (void)argc;
    (void)argv;
    XWINDOW win;
    win.title = (char *)"Terminal";
    win.width = BUSY_WIDTH;
    win.height = BUSY_HEIGHT;
    win.sets = XWIN_NORMAL | XWIN_SUPPORT_RESIZEABLE;
    xapi_CreateWindow(&g_window, &win);
    xapi_SetIcon(g_window, "/system/icon/terminal.png");
    SetMsgPrcor(g_window, busyterm_msg);
    calculate_terminal_geometry(BUSY_WIDTH, BUSY_HEIGHT, &g_term_cols, &g_term_rows);
    terminal_init();
    terminal_resize(BUSY_WIDTH, BUSY_HEIGHT);
    clear_window();
    xapi_RefreshWindow(g_window);

    if (spawn_busybox(envp) < 0)
    {
        char msg[96];
        snprintf(msg, sizeof(msg), "busyterm: failed to start shell at %s errno=%d\n",
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
                const char *msg = "\n[shell exited]\n";
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
