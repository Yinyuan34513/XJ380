#include <x3api.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <errno.h>
#include "texter.h"

static const int TXR_TOOLBAR_H = 40;
static const int TXR_STATUS_H = 24;
static const int TXR_MARGIN_X = 12;
static const int TXR_MARGIN_Y = 10;
static const int TXR_LINE_H = 16;
static const int TXR_CHAR_W = 8;
static const int TXR_MAX_PATH = 256;
static const int TXR_GROW_STEP = 4096;
static const UINT64 TXR_KEY_HOME = 151;
static const UINT64 TXR_KEY_UP = 152;
static const UINT64 TXR_KEY_PAGE_UP = 153;
static const UINT64 TXR_KEY_LEFT = 154;
static const UINT64 TXR_KEY_RIGHT = 155;
static const UINT64 TXR_KEY_END = 156;
static const UINT64 TXR_KEY_DOWN = 157;
static const UINT64 TXR_KEY_PAGE_DOWN = 158;
static const UINT64 TXR_KEY_INSERT = 159;
static const UINT64 TXR_KEY_DELETE = 160;

enum
{
    BTN_NEW = 101,
    BTN_OPEN = 102,
    BTN_SAVE = 103,
    BTN_SAVE_AS = 104
};

static HDLE g_handle = 0;
static int g_width = TXR_X;
static int g_height = TXR_Y;
static bool g_need_redraw = true;
static bool g_need_layout_redraw = true;
static bool g_need_text_redraw = true;
static bool g_need_status_redraw = true;
static bool g_running = true;

static char *g_text = NULL;
static int g_length = 0;
static int g_capacity = 0;
static int g_cursor = 0;
static int g_select_anchor = -1;
static int g_scroll_y = 0;
static int g_scroll_x = 0;
static bool g_dirty = false;

static char g_file_path[TXR_MAX_PATH] = {0};
static char g_status[160] = "Ready";

static int txr_min(int a, int b) { return a < b ? a : b; }
static int txr_max(int a, int b) { return a > b ? a : b; }

static void set_status(const char *text)
{
    if (text == NULL) text = "";
    strncpy(g_status, text, sizeof(g_status) - 1);
    g_status[sizeof(g_status) - 1] = '\0';
    g_need_status_redraw = true;
    g_need_redraw = true;
}

static void update_title()
{
    if (g_handle == 0) return;
    char title[320];
    const char *path = g_file_path[0] ? g_file_path : "Untitled";
    snprintf(title, sizeof(title), "%s%s - 文本编辑器", path, g_dirty ? " *" : "");
    xapi_SetWindowTitle(g_handle, title);
}

static bool ensure_capacity(int need)
{
    if (need <= g_capacity) return true;
    int new_cap = g_capacity > 0 ? g_capacity : TXR_GROW_STEP;
    while (new_cap < need) new_cap += TXR_GROW_STEP;
    char *new_buf = (char *)realloc(g_text, (size_t)new_cap);
    if (new_buf == NULL) {
        set_status("内存不足");
        return false;
    }
    g_text = new_buf;
    g_capacity = new_cap;
    return true;
}

static void mark_dirty(bool dirty)
{
    g_dirty = dirty;
    update_title();
    g_need_status_redraw = true;
    g_need_redraw = true;
}

static void clear_document()
{
    if (!ensure_capacity(1)) return;
    g_text[0] = '\0';
    g_length = 0;
    g_cursor = 0;
    g_select_anchor = -1;
    g_scroll_y = 0;
    g_scroll_x = 0;
    mark_dirty(false);
    g_need_text_redraw = true;
}

static bool save_to_path(const char *path)
{
    if (path == NULL || path[0] == '\0') {
        set_status("保存路径为空");
        return false;
    }

    int fd = open(path, O_CREAT | O_TRUNC | O_WRONLY, 0644);
    if (fd < 0) {
        set_status("无法打开文件保存");
        return false;
    }

    int wrote = 0;
    while (wrote < g_length) {
        int ret = write(fd, g_text + wrote, (size_t)(g_length - wrote));
        if (ret <= 0) {
            close(fd);
            set_status("写入失败");
            return false;
        }
        wrote += ret;
    }
    close(fd);

    strncpy(g_file_path, path, sizeof(g_file_path) - 1);
    g_file_path[sizeof(g_file_path) - 1] = '\0';
    mark_dirty(false);
    set_status("已保存");
    return true;
}

static bool load_from_path(const char *path)
{
    if (path == NULL || path[0] == '\0') {
        set_status("打开路径为空");
        return false;
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        set_status("无法打开文件");
        return false;
    }

    struct stat st;
    int expected = 0;
    if (fstat(fd, &st) == 0 && st.st_size > 0) expected = (int)st.st_size;
    if (!ensure_capacity(expected + 1)) {
        close(fd);
        return false;
    }

    int total = 0;
    while (true) {
        if (total + 1024 >= g_capacity && !ensure_capacity(total + 1025)) {
            close(fd);
            return false;
        }
        int got = read(fd, g_text + total, (size_t)(g_capacity - total - 1));
        if (got < 0) {
            close(fd);
            set_status("读取失败");
            return false;
        }
        if (got == 0) break;
        total += got;
    }
    close(fd);

    int normalized = 0;
    for (int i = 0; i < total; ++i) {
        if (g_text[i] == '\r') {
            if (i + 1 < total && g_text[i + 1] == '\n') continue;
            g_text[normalized++] = '\n';
            continue;
        }
        g_text[normalized++] = g_text[i];
    }

    g_text[normalized] = '\0';
    g_length = normalized;
    g_cursor = 0;
    g_select_anchor = -1;
    g_scroll_x = 0;
    g_scroll_y = 0;
    strncpy(g_file_path, path, sizeof(g_file_path) - 1);
    g_file_path[sizeof(g_file_path) - 1] = '\0';
    mark_dirty(false);
    set_status("已打开文件");
    g_need_text_redraw = true;
    return true;
}

static bool run_file_dialog(UINT64 mode, const char *title, char *path, int path_size)
{
    char result[TXR_MAX_PATH];
    memset(result, 0, sizeof(result));
    UINT64 ret = xapi_FileDialog(mode, (char *)title, path != NULL ? path : (char *)"", result, sizeof(result));
    if ((INT64)ret < 0) {
        set_status("文件对话框调用失败");
        return false;
    }
    if (result[0] == '\0') {
        set_status("已取消");
        return false;
    }

    strncpy(path, result, (size_t)path_size - 1);
    path[path_size - 1] = '\0';
    return true;
}

static bool do_save(bool save_as)
{
    char path[TXR_MAX_PATH];
    memset(path, 0, sizeof(path));
    if (!save_as && g_file_path[0] != '\0') {
        strncpy(path, g_file_path, sizeof(path) - 1);
    } else {
        strncpy(path, g_file_path[0] ? g_file_path : "/users", sizeof(path) - 1);
        if (!run_file_dialog(XAPI_FILE_DIALOG_SAVE, "另存为", path, sizeof(path))) return false;
    }
    return save_to_path(path);
}

static bool do_open()
{
    char path[TXR_MAX_PATH];
    strncpy(path, g_file_path[0] ? g_file_path : "/users", sizeof(path) - 1);
    path[sizeof(path) - 1] = '\0';
    if (!run_file_dialog(XAPI_FILE_DIALOG_OPEN, "打开文件", path, sizeof(path))) return false;
    return load_from_path(path);
}

static void do_new()
{
    g_file_path[0] = '\0';
    clear_document();
    set_status("新建文件");
}

static int visible_rows()
{
    int h = g_height - TXR_TOOLBAR_H - TXR_STATUS_H - TXR_MARGIN_Y * 2;
    int rows = h / TXR_LINE_H;
    return rows < 1 ? 1 : rows;
}

static int visible_cols()
{
    int w = g_width - TXR_MARGIN_X * 2;
    int cols = w / TXR_CHAR_W;
    return cols < 1 ? 1 : cols;
}

static void normalize_selection(int *start, int *end)
{
    if (g_select_anchor < 0 || g_select_anchor == g_cursor) {
        *start = -1;
        *end = -1;
        return;
    }
    *start = txr_min(g_select_anchor, g_cursor);
    *end = txr_max(g_select_anchor, g_cursor);
}

static void delete_selection()
{
    int sel_start;
    int sel_end;
    normalize_selection(&sel_start, &sel_end);
    if (sel_start < 0) return;

    memmove(g_text + sel_start, g_text + sel_end, (size_t)(g_length - sel_end + 1));
    g_length -= sel_end - sel_start;
    g_cursor = sel_start;
    g_select_anchor = -1;
    mark_dirty(true);
}

static void insert_bytes(const char *text, int len)
{
    if (text == NULL || len <= 0) return;
    delete_selection();
    if (!ensure_capacity(g_length + len + 1)) return;
    memmove(g_text + g_cursor + len, g_text + g_cursor, (size_t)(g_length - g_cursor + 1));
    memcpy(g_text + g_cursor, text, (size_t)len);
    g_cursor += len;
    g_length += len;
    mark_dirty(true);
}

static void delete_backward()
{
    int sel_start;
    int sel_end;
    normalize_selection(&sel_start, &sel_end);
    if (sel_start >= 0) {
        delete_selection();
        return;
    }
    if (g_cursor <= 0) return;
    memmove(g_text + g_cursor - 1, g_text + g_cursor, (size_t)(g_length - g_cursor + 1));
    g_cursor--;
    g_length--;
    mark_dirty(true);
}

static void cursor_line_col(int pos, int *line, int *col)
{
    int l = 0;
    int c = 0;
    if (pos < 0) pos = 0;
    if (pos > g_length) pos = g_length;
    for (int i = 0; i < pos; ++i) {
        if (g_text[i] == '\n') {
            l++;
            c = 0;
        } else {
            c++;
        }
    }
    *line = l;
    *col = c;
}

static int position_from_line_col(int target_line, int target_col)
{
    int line = 0;
    int col = 0;
    for (int i = 0; i < g_length; ++i) {
        if (line == target_line && col == target_col) return i;
        if (g_text[i] == '\n') {
            if (line == target_line) return i;
            line++;
            col = 0;
        } else {
            col++;
        }
    }
    return g_length;
}

static void ensure_cursor_visible()
{
    int line;
    int col;
    cursor_line_col(g_cursor, &line, &col);
    int rows = visible_rows();
    int cols = visible_cols();

    if (line < g_scroll_y) g_scroll_y = line;
    if (line >= g_scroll_y + rows) g_scroll_y = line - rows + 1;
    if (col < g_scroll_x) g_scroll_x = col;
    if (col >= g_scroll_x + cols) g_scroll_x = col - cols + 1;
    if (g_scroll_y < 0) g_scroll_y = 0;
    if (g_scroll_x < 0) g_scroll_x = 0;
}

static void move_cursor_to(int pos, bool keep_selection)
{
    if (pos < 0) pos = 0;
    if (pos > g_length) pos = g_length;
    if (!keep_selection) g_select_anchor = -1;
    else if (g_select_anchor < 0) g_select_anchor = g_cursor;
    g_cursor = pos;
    ensure_cursor_visible();
}

static int text_area_top() { return TXR_TOOLBAR_H + TXR_MARGIN_Y; }
static int text_area_bottom() { return g_height - TXR_STATUS_H - TXR_MARGIN_Y; }
static int text_area_left() { return TXR_MARGIN_X; }
static int text_area_right() { return g_width - TXR_MARGIN_X - 1; }

static void request_text_redraw()
{
    g_need_text_redraw = true;
    g_need_status_redraw = true;
    g_need_redraw = true;
}

static void render_layout()
{
    xapi_DrawRect(g_handle, 0, 0, g_width - 1, g_height - 1, 0xf7f4edff, true);
    xapi_DrawRect(g_handle, 0, 0, g_width - 1, TXR_TOOLBAR_H - 1, 0x335c67ff, true);
    xapi_DrawRect(g_handle, 0, TXR_TOOLBAR_H, g_width - 1, g_height - TXR_STATUS_H - 1, 0xfffdf8ff, true);
    xapi_DrawRect(g_handle, 0, g_height - TXR_STATUS_H, g_width - 1, g_height - 1, 0xe7dfd2ff, true);

    xapi_DeleteButton(g_handle, BTN_NEW);
    xapi_DeleteButton(g_handle, BTN_OPEN);
    xapi_DeleteButton(g_handle, BTN_SAVE);
    xapi_DeleteButton(g_handle, BTN_SAVE_AS);
    xapi_Button(g_handle, BTN_NEW, 12, 8, (char *)"新建");
    xapi_Button(g_handle, BTN_OPEN, 88, 8, (char *)"打开");
    xapi_Button(g_handle, BTN_SAVE, 164, 8, (char *)"保存");
    xapi_Button(g_handle, BTN_SAVE_AS, 240, 8, (char *)"另存为");

    g_need_layout_redraw = false;
    g_need_text_redraw = true;
    g_need_status_redraw = true;
}

static void render_text_area()
{
    xapi_DrawRect(g_handle, text_area_left(), text_area_top(), text_area_right(), text_area_bottom(), 0xfffdf8ff, true);

    int sel_start;
    int sel_end;
    normalize_selection(&sel_start, &sel_end);

    int line_no = 0;
    int col = 0;
    bool cursor_drawn = false;
    int rows = visible_rows();
    int cols = visible_cols();

    for (int i = 0; i <= g_length; ++i) {
        bool end = i == g_length;
        char ch = end ? '\0' : g_text[i];

        if (line_no >= g_scroll_y && line_no < g_scroll_y + rows) {
            int visible_col = col - g_scroll_x;
            if (!end && ch != '\n' && visible_col >= 0 && visible_col < cols) {
                int x = TXR_MARGIN_X + visible_col * TXR_CHAR_W;
                int y = text_area_top() + (line_no - g_scroll_y) * TXR_LINE_H;
                if (sel_start >= 0 && i >= sel_start && i < sel_end) {
                    xapi_DrawRect(g_handle, x - 1, y, x + TXR_CHAR_W, y + TXR_LINE_H - 1, 0xb7d7e8ff, true);
                }
                char buf[2] = {ch, '\0'};
                xapi_DrawSWText(g_handle, x, y, buf, 0x111111ff);
            }
            if (i == g_cursor && !cursor_drawn) {
                int cursor_x = TXR_MARGIN_X + (col - g_scroll_x) * TXR_CHAR_W;
                int cursor_y = text_area_top() + (line_no - g_scroll_y) * TXR_LINE_H;
                if (cursor_x < TXR_MARGIN_X) cursor_x = TXR_MARGIN_X;
                if (cursor_x > g_width - TXR_MARGIN_X) cursor_x = g_width - TXR_MARGIN_X;
                xapi_DrawLine(g_handle, cursor_x, cursor_y, cursor_x, cursor_y + TXR_LINE_H - 2, 0x005f73ff);
                cursor_drawn = true;
            }
        }

        if (end) break;
        if (ch == '\n') {
            line_no++;
            col = 0;
        } else {
            col++;
        }
    }

    if (!cursor_drawn) {
        ensure_cursor_visible();
    }

    g_need_text_redraw = false;
    xapi_RefreshPartWindow(g_handle, text_area_left(), text_area_top(), text_area_right(), text_area_bottom());
}

static void render_status_bar()
{
    xapi_DrawRect(g_handle, 0, g_height - TXR_STATUS_H, g_width - 1, g_height - 1, 0xe7dfd2ff, true);
    char info[256];
    int cursor_line;
    int cursor_col;
    cursor_line_col(g_cursor, &cursor_line, &cursor_col);
    snprintf(info, sizeof(info), "%s | 行 %d, 列 %d | %s",
             g_file_path[0] ? g_file_path : "Untitled",
             cursor_line + 1,
             cursor_col + 1,
             g_status);
    xapi_DrawSWText(g_handle, 8, g_height - 20, info, 0x3d405bff);
    g_need_status_redraw = false;
    xapi_RefreshPartWindow(g_handle, 0, g_height - TXR_STATUS_H, g_width - 1, g_height - 1);
}

static void render_editor()
{
    bool full_refresh = g_need_layout_redraw;
    if (g_need_layout_redraw) render_layout();
    if (g_need_text_redraw) render_text_area();
    if (g_need_status_redraw) render_status_bar();
    if (full_refresh) xapi_RefreshWindow(g_handle);
    g_need_redraw = false;
}

static int position_from_mouse(int x, int y)
{
    int row = (y - text_area_top()) / TXR_LINE_H;
    int col = (x - TXR_MARGIN_X) / TXR_CHAR_W;
    if (row < 0) row = 0;
    if (col < 0) col = 0;
    return position_from_line_col(g_scroll_y + row, g_scroll_x + col);
}

static void handle_char(char ch)
{
    if (ch == '\r' || ch < ' ') return;
    char buf[2] = {ch, '\0'};
    insert_bytes(buf, 1);
    ensure_cursor_visible();
    request_text_redraw();
}

static void move_cursor_left()
{
    int sel_start;
    int sel_end;
    normalize_selection(&sel_start, &sel_end);
    if (sel_start >= 0) {
        move_cursor_to(sel_start, false);
        return;
    }
    if (g_cursor > 0) move_cursor_to(g_cursor - 1, false);
}

static void move_cursor_right()
{
    int sel_start;
    int sel_end;
    normalize_selection(&sel_start, &sel_end);
    if (sel_start >= 0) {
        move_cursor_to(sel_end, false);
        return;
    }
    if (g_cursor < g_length) move_cursor_to(g_cursor + 1, false);
}

static void move_cursor_up()
{
    int line;
    int col;
    cursor_line_col(g_cursor, &line, &col);
    if (line > 0) move_cursor_to(position_from_line_col(line - 1, col), false);
}

static void move_cursor_down()
{
    int line;
    int col;
    cursor_line_col(g_cursor, &line, &col);
    move_cursor_to(position_from_line_col(line + 1, col), false);
}

static void move_cursor_home()
{
    int line;
    int col;
    cursor_line_col(g_cursor, &line, &col);
    (void)col;
    move_cursor_to(position_from_line_col(line, 0), false);
}

static void move_cursor_end()
{
    int pos = g_cursor;
    while (pos < g_length && g_text[pos] != '\n') pos++;
    move_cursor_to(pos, false);
}

static void delete_forward()
{
    int sel_start;
    int sel_end;
    normalize_selection(&sel_start, &sel_end);
    if (sel_start >= 0) {
        delete_selection();
        return;
    }
    if (g_cursor >= g_length) return;
    memmove(g_text + g_cursor, g_text + g_cursor + 1, (size_t)(g_length - g_cursor));
    g_length--;
    mark_dirty(true);
}

static void handle_special(UINT64 ch)
{
    if (ch == '\r') {
        return;
    } else if (ch == '\b') {
        delete_backward();
        ensure_cursor_visible();
        request_text_redraw();
    } else if (ch == '\n') {
        insert_bytes("\n", 1);
        ensure_cursor_visible();
        request_text_redraw();
    } else if (ch == TXR_KEY_LEFT) {
        move_cursor_left();
        request_text_redraw();
    } else if (ch == TXR_KEY_RIGHT) {
        move_cursor_right();
        request_text_redraw();
    } else if (ch == TXR_KEY_UP) {
        move_cursor_up();
        request_text_redraw();
    } else if (ch == TXR_KEY_DOWN) {
        move_cursor_down();
        request_text_redraw();
    } else if (ch == TXR_KEY_HOME) {
        move_cursor_home();
        request_text_redraw();
    } else if (ch == TXR_KEY_END) {
        move_cursor_end();
        request_text_redraw();
    } else if (ch == TXR_KEY_DELETE) {
        delete_forward();
        ensure_cursor_visible();
        request_text_redraw();
    } else if (ch == TXR_KEY_PAGE_UP) {
        g_scroll_y -= visible_rows();
        if (g_scroll_y < 0) g_scroll_y = 0;
        request_text_redraw();
    } else if (ch == TXR_KEY_PAGE_DOWN) {
        g_scroll_y += visible_rows();
        ensure_cursor_visible();
        request_text_redraw();
    }
}

static void handle_control(UINT64 id)
{
    switch (id) {
    case BTN_NEW:
        do_new();
        g_need_layout_redraw = true;
        g_need_redraw = true;
        break;
    case BTN_OPEN:
        if (do_open()) request_text_redraw();
        break;
    case BTN_SAVE:
        if (do_save(false)) request_text_redraw();
        break;
    case BTN_SAVE_AS:
        if (do_save(true)) request_text_redraw();
        break;
    default:
        break;
    }
}

static void texter_message_proc(UINT64 type, UINT64 hData, UINT64 lData)
{
    switch (type) {
    case MSG_CHAR:
        handle_char((char)lData);
        break;
    case MSG_SPCHAR:
        handle_special(lData);
        break;
    case MSG_LBUTTON:
        if ((int)lData >= text_area_top() && (int)lData < text_area_bottom()) {
            g_select_anchor = -1;
            move_cursor_to(position_from_mouse((int)hData, (int)lData), false);
            request_text_redraw();
        }
        break;
    case MSG_CRL:
        handle_control(hData);
        break;
    case MSG_ROLLER:
        g_scroll_y -= (int)hData * 3;
        if (g_scroll_y < 0) g_scroll_y = 0;
        request_text_redraw();
        break;
    case MSG_RESIZE:
        g_width = (int)hData;
        g_height = (int)lData;
        ensure_cursor_visible();
        g_need_layout_redraw = true;
        g_need_redraw = true;
        break;
    default:
        break;
    }
}

static int texter_main_impl(int argc, char *argv[], char *envp[])
{
    (void)envp;

    if (!ensure_capacity(TXR_GROW_STEP)) return -1;
    clear_document();

    if (argc > 1 && argv[1] != NULL && argv[1][0] != '\0') {
        load_from_path(argv[1]);
    } else if (argc == 1 && argv[0] != NULL && argv[0][0] != '\0') {
        load_from_path(argv[0]);
    }

    XWINDOW window;
    window.title = (char *)"文本编辑器";
    window.width = TXR_X;
    window.height = TXR_Y;
    window.sets = XWIN_NORMAL | XWIN_SUPPORT_RESIZEABLE;
    xapi_CreateWindow(&g_handle, &window);
    xapi_SetIcon(g_handle, "/system/icon/texter.png");
    SetMsgPrcor(g_handle, texter_message_proc);
    update_title();
    g_need_redraw = true;

    while (g_running) {
        if (g_need_redraw) render_editor();
        xapi_Sleep(16);
    }

    return 0;
}

extern "C" int texter_main_cpp(int argc, char *argv[], char *envp[]) asm("_Z4mainiPPcS0_");
extern "C" int texter_main_cpp(int argc, char *argv[], char *envp[])
{
    return texter_main_impl(argc, argv, envp);
}
