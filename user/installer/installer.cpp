#include <krlibc.h>
#include <x3api.h>
#include <installer_protocol.h>

#define COLOR_BG          0x0B3D91FF
#define COLOR_BG_2        0x0F5FB8FF
#define COLOR_PANEL       0xF7FBFFFF
#define COLOR_PANEL_LINE  0x9CC7F4FF
#define COLOR_TEXT        0x102A43FF
#define COLOR_MUTED       0x526A83FF
#define COLOR_WHITE       0xFFFFFFFF
#define COLOR_BLUE        0x136FDCFF
#define COLOR_BLUE_DARK   0x0B4AA2FF
#define COLOR_GREEN       0x1EAD63FF
#define COLOR_RED         0xD64545FF
#define COLOR_DISABLED    0x94A3B8FF

static HDLE g_window = 0;
static UINT64 g_width = 860;
static UINT64 g_height = 560;
static bool g_need_redraw = true;
static bool g_install_requested = false;
static int g_selected = 0;
static xj380_installer_disk_list g_disks;
static xj380_installer_progress g_progress;

static void set_local_failure(INT64 result, const char *stage, const char *detail)
{
    memset(&g_progress, 0, sizeof(g_progress));
    g_progress.state = XJ380_INSTALLER_FAILED;
    g_progress.percent = 0;
    g_progress.result = result;
    if (stage != NULL) strncpy(g_progress.stage, stage, sizeof(g_progress.stage) - 1);
    if (detail != NULL) strncpy(g_progress.detail, detail, sizeof(g_progress.detail) - 1);
    g_need_redraw = true;
}

static bool progress_equal(const xj380_installer_progress *a, const xj380_installer_progress *b)
{
    return memcmp(a, b, sizeof(*a)) == 0;
}

static void refresh_disks()
{
    memset(&g_disks, 0, sizeof(g_disks));
    xapi_InstallerEnumDisks(&g_disks);
    if (g_selected >= (int)g_disks.count) g_selected = g_disks.count > 0 ? 0 : -1;
    g_need_redraw = true;
}

static void bytes_to_text(UINT64 bytes, char *out, UINT64 out_size)
{
    if (out == NULL || out_size == 0) return;
    if (bytes >= 1024ULL * 1024ULL * 1024ULL)
    {
        UINT64 gb10 = (bytes * 10ULL) / (1024ULL * 1024ULL * 1024ULL);
        snprintf(out, out_size, "%llu.%llu GiB", gb10 / 10ULL, gb10 % 10ULL);
    }
    else if (bytes >= 1024ULL * 1024ULL)
    {
        UINT64 mb = bytes / (1024ULL * 1024ULL);
        snprintf(out, out_size, "%llu MiB", mb);
    }
    else
    {
        UINT64 kb = bytes / 1024ULL;
        snprintf(out, out_size, "%llu KiB", kb);
    }
}

static bool in_rect(UINT64 x, UINT64 y, int x1, int y1, int x2, int y2)
{
    return x >= (UINT64)x1 && x <= (UINT64)x2 && y >= (UINT64)y1 && y <= (UINT64)y2;
}

static int disk_row_at(UINT64 y)
{
    int start_y = 168;
    int row_h = 54;
    if (y < (UINT64)start_y) return -1;
    int idx = (int)((y - start_y) / row_h);
    if (idx < 0 || idx >= (int)g_disks.count) return -1;
    return idx;
}

static void draw_button(int x1, int y1, int x2, int y2, const char *text, UINT32 color)
{
    xapi_DrawRect(g_window, x1, y1, x2, y2, color, true);
    xapi_DrawRect(g_window, x1, y1, x2, y2, 0x00000028, false);
    xapi_DrawSWText(g_window, x1 + 18, y1 + 10, (char *)text, COLOR_WHITE);
}

static void draw_progress_bar(int x1, int y1, int x2, int y2, UINT32 percent)
{
    if (percent > 100) percent = 100;
    xapi_DrawRect(g_window, x1, y1, x2, y2, 0xDCEBFAFF, true);
    xapi_DrawRect(g_window, x1, y1, x2, y2, COLOR_PANEL_LINE, false);
    int fill = x1 + (int)(((UINT64)(x2 - x1) * percent) / 100ULL);
    if (fill > x1) xapi_DrawRect(g_window, x1, y1, fill, y2, COLOR_BLUE, true);
}

static void shorten_path(const char *src, char *dst, UINT64 dst_size)
{
    if (dst == NULL || dst_size == 0)
        return;
    dst[0] = '\0';
    if (src == NULL || src[0] == '\0') return;

    size_t len = strlen(src);
    if (len < dst_size)
    {
        strncpy(dst, src, (size_t)dst_size - 1);
        dst[dst_size - 1] = '\0';
        return;
    }

    if (dst_size <= 8)
    {
        strncpy(dst, src, (size_t)dst_size - 1);
        dst[dst_size - 1] = '\0';
        return;
    }

    size_t head = (size_t)((dst_size - 4) / 2);
    size_t tail = (size_t)dst_size - 4 - head;
    snprintf(dst, (size_t)dst_size, "%.*s...%.*s", (int)head, src, (int)tail, src + len - tail);
}

static void draw_queue_panel(int x, int y, int w, int h)
{
    xapi_DrawSWText(g_window, x, y, (char *)"文件安装队列", COLOR_TEXT);

    char queue_head[64];
    if (g_progress.queue_total == 0)
        snprintf(queue_head, sizeof(queue_head), "0 / 0");
    else
        snprintf(queue_head, sizeof(queue_head), "%u / %u", g_progress.queue_index, g_progress.queue_total);
    xapi_DrawSWText(g_window, x + w - 96, y, queue_head, COLOR_MUTED);

    int list_y = y + 28;
    if (g_progress.queue_count == 0)
    {
        xapi_DrawSWText(g_window, x, list_y, (char *)"正在整理文件列表。", COLOR_MUTED);
        return;
    }

    int usable_h = h - 28;
    int row_h = 18;
    int extra_h = 20;
    int rows_per_col = (usable_h - extra_h) / row_h;
    if (rows_per_col < 1) rows_per_col = 1;
    UINT32 visible_count = g_progress.queue_count;
    if (visible_count > (UINT32)(rows_per_col * 2)) visible_count = (UINT32)(rows_per_col * 2);

    UINT32 left_count = (visible_count + 1) / 2;
    UINT32 right_count = visible_count - left_count;
    int left_w = (w - 12) / 2;
    int right_x = x + left_w + 12;

    for (UINT32 i = 0; i < visible_count; i++)
    {
        char path_text[160];
        char line[200];
        shorten_path(g_progress.queue_items[i], path_text, sizeof(path_text));
        if (i == 0)
            snprintf(line, sizeof(line), "当前：%s", path_text);
        else
            snprintf(line, sizeof(line), "后续：%s", path_text);

        int col_x = (i < left_count) ? x : right_x;
        int row_i = (i < left_count) ? (int)i : (int)(i - left_count);
        UINT32 color = (i == 0) ? COLOR_BLUE_DARK : COLOR_MUTED;
        xapi_DrawSWText(g_window, col_x, list_y + row_i * row_h, line, color);
    }

    if (g_progress.queue_count > visible_count)
    {
        UINT32 remain = g_progress.queue_count - visible_count;
        char more[64];
        snprintf(more, sizeof(more), "还有 %u 个文件未显示。", remain);
        int more_y = list_y + ((right_count > left_count) ? (int)right_count : (int)left_count) * row_h + 2;
        xapi_DrawSWText(g_window, x, more_y, more, COLOR_MUTED);
    }
}

static void draw_disk_rows(int x, int y, int w)
{
    if (g_disks.count == 0)
    {
        xapi_DrawSWText(g_window, x + 20, y + 20, (char *)"未发现可安装的硬盘。请确认 QEMU 已挂载目标磁盘。", COLOR_MUTED);
        return;
    }

    for (UINT32 i = 0; i < g_disks.count; i++)
    {
        int row_y = y + (int)i * 54;
        UINT32 bg = ((int)i == g_selected) ? 0xD9ECFFFF : COLOR_WHITE;
        xapi_DrawRect(g_window, x, row_y, x + w, row_y + 44, bg, true);
        xapi_DrawRect(g_window, x, row_y, x + w, row_y + 44, COLOR_PANEL_LINE, false);

        char line[160];
        char size_text[48];
        bytes_to_text(g_disks.disks[i].size_bytes, size_text, sizeof(size_text));
        snprintf(line, sizeof(line), "%s  |  %s  |  扇区 %u 字节",
                 g_disks.disks[i].name, size_text, g_disks.disks[i].sector_size);
        xapi_DrawSWText(g_window, x + 16, row_y + 13, line, COLOR_TEXT);
    }
}

static void render()
{
    xapi_DrawRect(g_window, 0, 0, (UINT32)g_width - 1, (UINT32)g_height - 1, COLOR_BG, true);
    xapi_DrawRect(g_window, 0, 0, (UINT32)g_width - 1, 86, COLOR_BG_2, true);
    xapi_DrawSWText(g_window, 28, 24, (char *)"XJ380 安装程序", COLOR_WHITE);
    xapi_DrawSWText(g_window, 28, 54, (char *)"请选择目标硬盘，安装程序会格式化该硬盘并写入系统文件。", 0xE7F3FFFF);

    int margin = 28;
    int top = 108;
    int left_w = (int)g_width / 2 - 42;
    int right_x = margin + left_w + 28;
    int right_w = (int)g_width - right_x - margin;
    int panel_h = (int)g_height - top - 28;

    xapi_DrawRect(g_window, margin, top, margin + left_w, top + panel_h, COLOR_PANEL, true);
    xapi_DrawRect(g_window, margin, top, margin + left_w, top + panel_h, COLOR_PANEL_LINE, false);
    xapi_DrawSWText(g_window, margin + 18, top + 18, (char *)"目标硬盘", COLOR_TEXT);
    draw_disk_rows(margin + 18, top + 60, left_w - 36);

    xapi_DrawRect(g_window, right_x, top, right_x + right_w, top + panel_h, COLOR_PANEL, true);
    xapi_DrawRect(g_window, right_x, top, right_x + right_w, top + panel_h, COLOR_PANEL_LINE, false);
    xapi_DrawSWText(g_window, right_x + 18, top + 18, (char *)"安装进度", COLOR_TEXT);
    xapi_DrawSWText(g_window, right_x + 18, top + 58, g_progress.stage, COLOR_BLUE_DARK);
    xapi_DrawSWText(g_window, right_x + 18, top + 88, g_progress.detail, COLOR_MUTED);

    draw_progress_bar(right_x + 18, top + 136, right_x + right_w - 18, top + 164, g_progress.percent);
    char percent_text[32];
    snprintf(percent_text, sizeof(percent_text), "%u%%", g_progress.percent);
    xapi_DrawSWText(g_window, right_x + 18, top + 176, percent_text, COLOR_MUTED);

    draw_queue_panel(right_x + 18, top + 216, right_w - 36, panel_h - 300);

    if (g_progress.state == XJ380_INSTALLER_DONE)
        xapi_DrawSWText(g_window, right_x + 18, top + panel_h - 84, (char *)"安装完成后请关闭虚拟机，移除 ISO，再从硬盘启动。", COLOR_GREEN);
    else if (g_progress.state == XJ380_INSTALLER_FAILED)
    {
        char error_line[128];
        snprintf(error_line, sizeof(error_line), "安装失败，错误码: %lld", (long long)g_progress.result);
        xapi_DrawSWText(g_window, right_x + 18, top + panel_h - 84, error_line, COLOR_RED);
        xapi_DrawSWText(g_window, right_x + 18, top + panel_h - 56, (char *)"请同时查看 serial.log 中 installer: failed 行。", COLOR_MUTED);
    }
    else
        xapi_DrawSWText(g_window, right_x + 18, top + panel_h - 84, (char *)"提示：安装会清空目标硬盘上的所有数据。", COLOR_MUTED);

    UINT32 start_color = (g_selected >= 0 && !g_install_requested) ? COLOR_BLUE : COLOR_DISABLED;
    draw_button(right_x + 18, top + panel_h - 58, right_x + 178, top + panel_h - 22, "开始安装", start_color);
    draw_button(right_x + 196, top + panel_h - 58, right_x + 326, top + panel_h - 22, "刷新硬盘", COLOR_BLUE_DARK);

    xapi_RefreshWindow(g_window);
    g_need_redraw = false;
}

static void start_install()
{
    if (g_selected < 0 || g_selected >= (int)g_disks.count || g_install_requested) return;
    INT64 ret = (INT64)xapi_InstallerStart(g_disks.disks[g_selected].id);
    if (ret < 0)
    {
        set_local_failure(ret, "无法开始安装", "内核拒绝启动安装任务，请确认当前是 ISO 安装模式。");
        return;
    }
    g_install_requested = true;
    g_need_redraw = true;
}

static void message_handler(UINT64 type, UINT64 hData, UINT64 lData)
{
    switch (type)
    {
    case MSG_LBUTTON:
    {
        int top = 108;
        int left_w = (int)g_width / 2 - 42;
        int right_x = 28 + left_w + 28;
        int panel_h = (int)g_height - top - 28;
        int row = disk_row_at(lData);
        if (row >= 0 && hData >= 46 && hData <= (UINT64)(28 + left_w - 18))
        {
            g_selected = row;
            g_need_redraw = true;
        }
        else if (in_rect(hData, lData, right_x + 18, top + panel_h - 58, right_x + 178, top + panel_h - 22))
        {
            start_install();
        }
        else if (in_rect(hData, lData, right_x + 196, top + panel_h - 58, right_x + 326, top + panel_h - 22))
        {
            if (!g_install_requested) refresh_disks();
        }
        break;
    }
    case MSG_RESIZE:
        g_width = hData;
        g_height = lData;
        g_need_redraw = true;
        break;
    default:
        break;
    }
}

static int installer_main_impl(int argc, char *argv[], char *envp[])
{
    (void)argc;
    (void)argv;
    (void)envp;

    memset(&g_progress, 0, sizeof(g_progress));
    g_progress.state = XJ380_INSTALLER_IDLE;
    strcpy(g_progress.stage, "等待安装");
    strcpy(g_progress.detail, "请选择目标硬盘，然后点击开始安装。");

    XWINDOW window;
    window.width = 0;
    window.height = 0;
    window.title = (char *)"XJ380 安装程序";
    window.sets = XWIN_FULL_SCR;
    xapi_CreateWindow(&g_window, &window);
    xapi_GetWindowSize(g_window, &g_width, &g_height);
    SetMsgPrcor(g_window, message_handler);
    refresh_disks();

    while (true)
    {
        if (g_install_requested)
        {
            xj380_installer_progress latest;
            memset(&latest, 0, sizeof(latest));
            if ((INT64)xapi_InstallerProgress(&latest) >= 0 && !progress_equal(&latest, &g_progress))
            {
                memcpy(&g_progress, &latest, sizeof(g_progress));
                g_need_redraw = true;
            }
        }
        if (g_need_redraw) render();
        xapi_Sleep(100);
    }
    return 0;
}

extern "C" int installer_main_cpp(int argc, char *argv[], char *envp[]) asm("_Z4mainiPPcS0_");
extern "C" int installer_main_cpp(int argc, char *argv[], char *envp[])
{
    return installer_main_impl(argc, argv, envp);
}

extern "C" int main(int argc, char *argv[], char *envp[])
{
    return installer_main_impl(argc, argv, envp);
}
