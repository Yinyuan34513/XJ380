#include <x3api.h>
#include <libsys.h>
#include <krlibc.h>
#include "cm_proto.h"
#include "../../kernel/build_settings.h"


int cindex = 1;

bool dont_refresh = false;
bool about_memory_show_mb = false;

static XCOLORA *ctrlmenu_background_cache = NULL;
static UINT64   ctrlmenu_background_width = 0;
static UINT64   ctrlmenu_background_height = 0;

char wdaystr_table[7][20] = {
    "星期日",
    "星期一",
    "星期二",
    "星期三",
    "星期四",
    "星期五",
    "星期六",
};

static void draw_background_region(UINT64 refresh_x1, UINT64 refresh_y1, UINT64 refresh_x2, UINT64 refresh_y2);

void init_ctrlmenu_background_cache()
{
    if (win_width == 0 || win_height == 0) return;
    if (ctrlmenu_background_cache != NULL && ctrlmenu_background_width == win_width &&
        ctrlmenu_background_height == win_height)
        return;

    if (ctrlmenu_background_cache != NULL)
    {
        free(ctrlmenu_background_cache);
        ctrlmenu_background_cache = NULL;
    }

    size_t pixel_count = (size_t)win_width * (size_t)win_height;
    if (pixel_count == 0 || pixel_count > (size_t)-1 / sizeof(XCOLORA)) return;

    ctrlmenu_background_cache = (XCOLORA *)malloc(pixel_count * sizeof(XCOLORA));
    if (ctrlmenu_background_cache == NULL) return;

    if (!xapi_LoadPicture(ctrlmenu_background_cache, (UINT32)win_width, (UINT32)win_height,
                          (char *)"/system/resources/image/background3.png"))
    {
        free(ctrlmenu_background_cache);
        ctrlmenu_background_cache = NULL;
        ctrlmenu_background_width = 0;
        ctrlmenu_background_height = 0;
        return;
    }

    ctrlmenu_background_width = win_width;
    ctrlmenu_background_height = win_height;
}


static const UINT32 CTRLMENU_BLUE        = 0x00a2e8ff;
static const UINT32 CTRLMENU_PANEL       = 0x12324af2;
static const UINT32 CTRLMENU_PANEL_SOFT  = 0x1d4c69ee;
static const UINT32 CTRLMENU_CARD        = 0x173c58e8;
static const UINT32 CTRLMENU_CARD_ACTIVE = 0x00a2e8ff;
static const UINT32 CTRLMENU_WHITE       = 0xffffffff;
static const UINT32 CTRLMENU_TEXT_DIM    = 0xffc8f7ff;
static const UINT32 CTRLMENU_TEXT_MUTED  = 0xb8d8e8ff;

enum CtrlmenuAppCategory
{
    CM_APP_FAVORITE = 1 << 0,
    CM_APP_SYSTEM   = 1 << 1,
    CM_APP_CREATE   = 1 << 2,
    CM_APP_NETWORK  = 1 << 3,
    CM_APP_DEV      = 1 << 4,
    CM_APP_MEDIA    = 1 << 5,
};

struct CtrlmenuAppEntry
{
    char   *title;
    char   *desc;
    char   *path;
    char   *icon;
    UINT32  categories;
    int     setting_page;
};

struct CtrlmenuAppCategoryEntry
{
    char  *title;
    UINT32 category;
};

static CtrlmenuAppEntry g_ctrlmenu_apps[] = {
    {(char *)"文件管理器", (char *)"浏览磁盘、用户目录和系统文件", (char *)"/apps/system/fmanager.elf",
     (char *)"folder-open", CM_APP_FAVORITE | CM_APP_SYSTEM, 0},
    {(char *)"终端", (char *)"打开原生命令行 Shell", (char *)"/apps/system/shell.elf", (char *)"file-lines",
     CM_APP_FAVORITE | CM_APP_SYSTEM | CM_APP_DEV, 0},
    {(char *)"任务管理器", (char *)"查看进程、窗口和系统任务", (char *)"/apps/system/taskmgr.elf", (char *)"gear",
     CM_APP_FAVORITE | CM_APP_SYSTEM, 0},
    {(char *)"系统设置", (char *)"账户、外观、网络、时间与设备", NULL, (char *)"gear",
     CM_APP_FAVORITE | CM_APP_SYSTEM, 1},
    {(char *)"关于 XJ380", (char *)"查看系统版本和版权信息", (char *)"/apps/system/xjver.elf", (char *)"user",
     CM_APP_FAVORITE | CM_APP_SYSTEM, 0},
    {(char *)"计算器", (char *)"基础计算工具", (char *)"/apps/builtin/calc.elf", (char *)"file",
     CM_APP_FAVORITE, 0},
    {(char *)"文本编辑器", (char *)"编辑文本和配置文件", (char *)"/apps/builtin/texter.elf", (char *)"markdown",
     CM_APP_FAVORITE | CM_APP_CREATE | CM_APP_DEV, 0},
    {(char *)"图片查看器", (char *)"查看图片文件", (char *)"/apps/builtin/picturer.elf", (char *)"file-image",
     CM_APP_CREATE | CM_APP_MEDIA, 0},
    {(char *)"浏览器", (char *)"浏览网页和 HTTP 内容", (char *)"/apps/builtin/browser.elf", (char *)"file",
     CM_APP_NETWORK, 0},
    {(char *)"Nut", (char *)"脚本和自动化运行环境", (char *)"/apps/builtin/nut.elf", (char *)"file-lines",
     CM_APP_DEV, 0},
    {(char *)"兼容终端", (char *)"Linux 兼容层终端", (char *)"/apps/busyterm.elf", (char *)"file-lines",
     CM_APP_DEV, 0},
};

static CtrlmenuAppCategoryEntry g_ctrlmenu_categories[] = {
    {(char *)"全部", 0},
    {(char *)"常用", CM_APP_FAVORITE},
    {(char *)"系统", CM_APP_SYSTEM},
    {(char *)"创作", CM_APP_CREATE},
    {(char *)"网络", CM_APP_NETWORK},
    {(char *)"开发", CM_APP_DEV},
    {(char *)"媒体", CM_APP_MEDIA},
};

static int g_app_showcase_category = 0;
static char   *g_last_launch_path = NULL;
static UINT64  g_last_launch_time = 0;

static int ctrlmenu_app_count()
{
    return (int)(sizeof(g_ctrlmenu_apps) / sizeof(g_ctrlmenu_apps[0]));
}

static int ctrlmenu_category_count()
{
    return (int)(sizeof(g_ctrlmenu_categories) / sizeof(g_ctrlmenu_categories[0]));
}

static bool ctrlmenu_point_in_rect(int x, int y, int x1, int y1, int x2, int y2)
{
    return x >= x1 && x <= x2 && y >= y1 && y <= y2;
}

static void ctrlmenu_draw_card(int x1, int y1, int x2, int y2, UINT32 fill)
{
    xapi_DrawRect(handle, x1, y1, x2, y2, fill, true);
    xapi_DrawRect(handle, x1, y1, x2, y1 + 2, CTRLMENU_BLUE, true);
}

static void ctrlmenu_draw_app_icon(int x, int y, char *icon)
{
    xapi_DrawRect(handle, x, y, x + 39, y + 39, CTRLMENU_PANEL_SOFT, true);
    xapi_DrawFA(handle, x + 10, y + 9, 19, icon);
}

static void ctrlmenu_draw_app_card(const CtrlmenuAppEntry *app, int x1, int y1, int x2, int y2)
{
    ctrlmenu_draw_card(x1, y1, x2, y2, CTRLMENU_CARD);
    ctrlmenu_draw_app_icon(x1 + 14, y1 + 18, app->icon);
    xapi_DrawText(handle, x1 + 66, y1 + 14, app->title, 16, CTRLMENU_WHITE);
    xapi_DrawText(handle, x1 + 66, y1 + 46, app->desc, 11, CTRLMENU_TEXT_DIM);
}

static void ctrlmenu_draw_status_row(int x, int y, char *name, char *value)
{
    xapi_DrawText(handle, x, y, name, 11, CTRLMENU_TEXT_MUTED);
    xapi_DrawText(handle, x + 92, y, value, 11, CTRLMENU_WHITE);
}

static void ctrlmenu_format_memory(char *buffer, size_t size)
{
    if (buffer == NULL || size == 0) return;
    memset(buffer, 0, size);

    UINT64 memory_mib = xapi_GetMemorySize() / 1024;
    if (memory_mib >= 1024)
    {
        strcat(buffer, xcr_int2char(memory_mib / 1024));
        strcat(buffer, " GB");
    }
    else
    {
        strcat(buffer, xcr_int2char(memory_mib));
        strcat(buffer, " MB");
    }
}

static void ctrlmenu_format_resolution(char *buffer, size_t size)
{
    if (buffer == NULL || size == 0) return;
    memset(buffer, 0, size);
    strcat(buffer, xcr_int2char(win_width));
    strcat(buffer, " x ");
    strcat(buffer, xcr_int2char(win_height));
}

static void ctrlmenu_format_datetime(char *buffer, size_t size)
{
    if (buffer == NULL || size == 0) return;
    memset(buffer, 0, size);

    TimeType tm;
    xapi_GetTimeX(&tm);
    sprintf(buffer, "%d-%02u-%02u %02u:%02u", tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min);
}

static void ctrlmenu_launch_app(const CtrlmenuAppEntry *app)
{
    if (app == NULL) return;

    UINT64 now = xapi_GetTimeNano();
    char *launch_key = app->path != NULL ? app->path : app->title;
    if (g_last_launch_path == launch_key && now >= g_last_launch_time &&
        now - g_last_launch_time < 300000000ULL)
    {
        return;
    }
    g_last_launch_path = launch_key;
    g_last_launch_time = now;

    if (app->setting_page > 0)
    {
        cindex = 3;
        setting_cindex = app->setting_page;
        draw_background();
        return;
    }

    if (app->path != NULL) xapi_Run(app->path);
}

static void ctrlmenu_home_app_rect(int index, int *x1, int *y1, int *x2, int *y2)
{
    int left   = 24;
    int right  = (int)win_width - 24;
    int top    = (win_height < 700) ? 300 : 312;
    int gap    = (win_height < 700) ? 10 : 16;
    int height = (win_height < 700) ? 72 : 82;
    int cols   = (win_width >= 1120) ? 3 : 2;
    int width  = (right - left - gap * (cols - 1)) / cols;

    *x1 = left + (index % cols) * (width + gap);
    *y1 = top + (index / cols) * (height + gap);
    *x2 = *x1 + width;
    *y2 = *y1 + height;
}

static bool ctrlmenu_app_visible_in_showcase(const CtrlmenuAppEntry *app)
{
    UINT32 category = g_ctrlmenu_categories[g_app_showcase_category].category;
    return category == 0 || (app->categories & category) != 0;
}

static void ctrlmenu_showcase_app_rect(int visible_index, int *x1, int *y1, int *x2, int *y2)
{
    int left   = 224;
    int right  = (int)win_width - 24;
    int top    = 202;
    int gap    = (win_height < 700) ? 8 : 14;
    int height = (win_height < 700) ? 62 : 86;
    int cols   = (right - left >= 900) ? 3 : 2;
    int width  = (right - left - gap * (cols - 1)) / cols;

    *x1 = left + (visible_index % cols) * (width + gap);
    *y1 = top + (visible_index / cols) * (height + gap);
    *x2 = *x1 + width;
    *y2 = *y1 + height;
}

void draw_background()
{
    draw_background_region(0, 0, win_width, win_height);
}

void draw_background_body()
{
    draw_background_region(0, 121, win_width, win_height);
}

void draw_background_time()
{
    draw_background_region(900, 18, win_width - 8, 68);
}

static void draw_background_region(UINT64 refresh_x1, UINT64 refresh_y1, UINT64 refresh_x2, UINT64 refresh_y2)
{
    if (dont_refresh) return;

    xapi_DrawRect(handle, 0, 0, win_width - 1, win_height - 1, 0x4d4d4dea, true);
    if (ctrlmenu_background_cache != NULL && ctrlmenu_background_width == win_width &&
        ctrlmenu_background_height == win_height)
    {
        xapi_WriteBufferA(handle, 0, 0, (UINT32)win_width, (UINT32)win_height, ctrlmenu_background_cache);
    }
    else
    {
        xapi_DrawPicture(handle, 0, 0, win_width, win_height, "/system/resources/image/background3.png");
    }
    xapi_DrawText(handle, 30, 18, "控制中心", 40, 0xffffffff);
    xapi_DrawRect(handle, 0, 116, win_width - 1, 120, 0x00a2e8ff, true);

    if (cindex == 1) 
    {
        xapi_DrawRect(handle, 280, 60, 386, 120, 0x00a2e8ff, true);
        draw_mainpage();
    }
    else if (cindex == 2) 
    {
        xapi_DrawRect(handle, 386, 60, 544, 120, 0x00a2e8ff, true);
        draw_app_showcase();
    }
    else if (cindex == 3)
    {
        xapi_DrawRect(handle, 544, 60, 670, 120, 0x00a2e8ff, true);
        draw_setting();
    }

    xapi_DrawText(handle, 312, 64, "主页", 16, 0xffffffff);
    xapi_DrawText(handle, 412, 64, "应用陈列柜", 16, 0xffffffff);
    xapi_DrawText(handle, 576, 64, "控制台", 16, 0xffffffff);

    char time_str_buffer[64];
    memset(time_str_buffer, 0, 64);

    TimeType tm;
    xapi_GetTimeX(&tm);

    sprintf(time_str_buffer, "%d 年 %d 月 %d 日 %s %02u:%02u", tm.tm_year, tm.tm_mon, tm.tm_mday, 
                    wdaystr_table[tm.tm_wday - 1], tm.tm_hour, tm.tm_min);
    xapi_DrawText(handle, 940, 44, time_str_buffer, 16, 0xffffffff);

    xapi_RefreshPartWindow(handle, refresh_x1, refresh_y1, refresh_x2, refresh_y2);
}

void draw_mainpage()
{
    UserInfo user_info;
    memset(&user_info, 0, sizeof(user_info));
    xapi_GetCurrentUser(&user_info);

    char system_version[64];
    char memory_size[64];
    char resolution[64];
    char datetime[64];
    memset(system_version, 0, sizeof(system_version));
    xapi_GetSystemVersion(system_version);
    ctrlmenu_format_memory(memory_size, sizeof(memory_size));
    ctrlmenu_format_resolution(resolution, sizeof(resolution));
    ctrlmenu_format_datetime(datetime, sizeof(datetime));

    int user_x1 = 24;
    int user_y1 = 140;
    int user_x2 = 344;
    int user_y2 = 260;
    ctrlmenu_draw_card(user_x1, user_y1, user_x2, user_y2, CTRLMENU_PANEL);
    ctrlmenu_draw_app_icon(user_x1 + 18, user_y1 + 32, (char *)"user");
    xapi_DrawText(handle, user_x1 + 76, user_y1 + 24, "当前账户", 11, CTRLMENU_TEXT_DIM);
    xapi_DrawText(handle, user_x1 + 76, user_y1 + 54, user_info.name, 18, CTRLMENU_WHITE);

    int system_x1 = 368;
    int system_y1 = 140;
    int system_x2 = (int)win_width - 24;
    int system_y2 = 260;
    ctrlmenu_draw_card(system_x1, system_y1, system_x2, system_y2, CTRLMENU_PANEL);
    xapi_DrawText(handle, system_x1 + 18, system_y1 + 16, "XJ380 操作系统", 18, CTRLMENU_WHITE);
    xapi_DrawText(handle, system_x1 + 18, system_y1 + 48, OS_VERSION, 11, CTRLMENU_TEXT_DIM);
    ctrlmenu_draw_status_row(system_x1 + 18, system_y1 + 80, "系统版本", system_version);
    ctrlmenu_draw_status_row(system_x1 + 310, system_y1 + 80, "内核版本", (char *)KN_VERSION);

    xapi_DrawText(handle, 24, 282, "常用入口", 16, CTRLMENU_WHITE);
    int home_apps[] = {0, 1, 2, 3, 4, 5};
    for (int i = 0; i < (int)(sizeof(home_apps) / sizeof(home_apps[0])); i++)
    {
        int x1, y1, x2, y2;
        ctrlmenu_home_app_rect(i, &x1, &y1, &x2, &y2);
        if (y2 < (int)win_height - 44) ctrlmenu_draw_app_card(&g_ctrlmenu_apps[home_apps[i]], x1, y1, x2, y2);
    }

    int info_y = (win_width >= 1120) ? 508 : 606;
    int info_x1 = 24;
    int info_x2 = (int)win_width - 24;
    if (info_y + 86 < (int)win_height - 50)
    {
        ctrlmenu_draw_card(info_x1, info_y, info_x2, info_y + 86, CTRLMENU_PANEL);
        xapi_DrawText(handle, info_x1 + 16, info_y + 14, "系统状态", 16, CTRLMENU_WHITE);
        ctrlmenu_draw_status_row(info_x1 + 16, info_y + 48, "运行内存", memory_size);
        ctrlmenu_draw_status_row(info_x1 + 260, info_y + 48, "分辨率", resolution);
        ctrlmenu_draw_status_row(info_x1 + 504, info_y + 48, "当前时间", datetime);
    }

    xapi_DrawRect(handle, 8, win_height - 38, win_width - 8, win_height - 8, 0x00a2e8ff, true);
    xapi_DrawText(handle, 16, win_height - 38, "→ 返回桌面", 16, 0xffffffff);
}

void draw_app_showcase()
{
    xapi_DrawText(handle, 24, 136, "应用陈列柜", 24, CTRLMENU_WHITE);
    xapi_DrawText(handle, 24, 176, "选择分类或直接启动应用", 11, CTRLMENU_TEXT_DIM);

    int category_x1 = 24;
    int category_x2 = 188;
    int category_y  = 220;
    for (int i = 0; i < ctrlmenu_category_count(); i++)
    {
        int y1 = category_y + i * 42;
        int y2 = y1 + 30;
        UINT32 color = (i == g_app_showcase_category) ? CTRLMENU_CARD_ACTIVE : CTRLMENU_CARD;
        xapi_DrawRect(handle, category_x1, y1, category_x2, y2, color, true);
        xapi_DrawText(handle, category_x1 + 14, y1 + 5, g_ctrlmenu_categories[i].title, 11, CTRLMENU_WHITE);
    }

    int panel_x1 = 208;
    int panel_y1 = 136;
    int panel_x2 = (int)win_width - 24;
    int panel_y2 = (int)win_height - 24;
    ctrlmenu_draw_card(panel_x1, panel_y1, panel_x2, panel_y2, CTRLMENU_PANEL);
    xapi_DrawText(handle, panel_x1 + 16, panel_y1 + 14, g_ctrlmenu_categories[g_app_showcase_category].title, 18,
                  CTRLMENU_WHITE);
    xapi_DrawText(handle, panel_x1 + 16, panel_y1 + 48, "点击卡片启动应用；系统设置会在当前控制中心内打开。",
                  11, CTRLMENU_TEXT_DIM);

    int visible_index = 0;
    for (int i = 0; i < ctrlmenu_app_count(); i++)
    {
        if (!ctrlmenu_app_visible_in_showcase(&g_ctrlmenu_apps[i])) continue;

        int x1, y1, x2, y2;
        ctrlmenu_showcase_app_rect(visible_index, &x1, &y1, &x2, &y2);
        if (y2 < (int)win_height - 30)
        {
            ctrlmenu_draw_app_card(&g_ctrlmenu_apps[i], x1, y1, x2, y2);
        }
        visible_index++;
    }
}

bool ctrlmenu_handle_mainpage_click(int x, int y)
{
    if (x > 8 && y > (int)win_height - 38 && x < (int)win_width - 8 && y < (int)win_height - 8)
    {
        exit_cm = true;
        xapi_CloseWindow(handle);
        return true;
    }

    int home_apps[] = {0, 1, 2, 3, 4, 5};
    for (int i = 0; i < (int)(sizeof(home_apps) / sizeof(home_apps[0])); i++)
    {
        int x1, y1, x2, y2;
        ctrlmenu_home_app_rect(i, &x1, &y1, &x2, &y2);
        if (y2 < (int)win_height - 44 && ctrlmenu_point_in_rect(x, y, x1, y1, x2, y2))
        {
            ctrlmenu_launch_app(&g_ctrlmenu_apps[home_apps[i]]);
            return true;
        }
    }

    return false;
}

bool ctrlmenu_handle_app_showcase_click(int x, int y)
{
    int category_x1 = 24;
    int category_x2 = 188;
    int category_y  = 220;
    for (int i = 0; i < ctrlmenu_category_count(); i++)
    {
        int y1 = category_y + i * 42;
        int y2 = y1 + 30;
        if (ctrlmenu_point_in_rect(x, y, category_x1, y1, category_x2, y2))
        {
            g_app_showcase_category = i;
            draw_background_body();
            return true;
        }
    }

    int visible_index = 0;
    for (int i = 0; i < ctrlmenu_app_count(); i++)
    {
        if (!ctrlmenu_app_visible_in_showcase(&g_ctrlmenu_apps[i])) continue;

        int x1, y1, x2, y2;
        ctrlmenu_showcase_app_rect(visible_index, &x1, &y1, &x2, &y2);
        if (y2 < (int)win_height - 30 && ctrlmenu_point_in_rect(x, y, x1, y1, x2, y2))
        {
            ctrlmenu_launch_app(&g_ctrlmenu_apps[i]);
            return true;
        }
        visible_index++;
    }

    return false;
}

int setting_cindex = 1;

char setting_apps_change_char_buffer[256];

char *charbox_buf = NULL;
int charbox_input_complete = 0;
static UINT64 charbox_input_id = 0;

int change_setting_apps_id = -1;

void create_input_box(char *buf)
{
    dont_refresh = true;
    xapi_DrawRect(handle, win_width / 2 - 300, win_height / 2 - 50, win_width / 2 + 300, win_height / 2 + 50, 0x007accff, true);
    xapi_DrawText(handle, win_width / 2 - 300 + 8, win_height / 2 - 50 + 4, "输入新的值：", 11, 0xffffffff);
    xapi_Button(handle, 114514, win_width / 2 + 300 - 58, win_height / 2 + 50 - 32, "取消");
    xapi_Button(handle, 114515, win_width / 2 + 300 - 116, win_height / 2 + 50 - 32, "确定");
    charbox_input_id = xapi_PutTextInputBox(handle, win_width / 2 - 300 + 8, win_height / 2 - 12, 584, buf);
    xapi_RefreshPartWindow(handle, win_width / 2 - 300, win_height / 2 - 50, win_width / 2 + 300, win_height / 2 + 50);

    charbox_buf = buf;
}

void delete_input_box(bool save)
{
    if (save && charbox_buf != NULL && charbox_input_id != 0)
    {
        xapi_GetTextInputBox(charbox_input_id, charbox_buf);
    }
    if (charbox_input_id != 0)
    {
        xapi_DeleteTextInputBox(charbox_input_id);
        charbox_input_id = 0;
    }
    charbox_input_complete = save ? 1 : -1;
    charbox_buf = NULL;
    dont_refresh = false;
    draw_background_body();
}

bool waiting_change_setting_apps = false;

void change_setting_apps(int id)
{
    memset(setting_apps_change_char_buffer, 0, sizeof(setting_apps_change_char_buffer));
    UserInfo usif;
    xapi_GetCurrentUser(&usif);
    char runfile_path[256];
    memset((void *)runfile_path, 0, 256);
    strcat(runfile_path, "/users/");
    strcat(runfile_path, usif.name);
    strcat(runfile_path, "/runfile.dat");
    XFILE *f = xapi_OpenFile(runfile_path);
    if (f)
    {
        RunfileSettings_Format *rff = (RunfileSettings_Format *)f->buffer;
        strcpy(setting_apps_change_char_buffer, rff->items[id].runpath);
        xapi_CloseFile(f);
    }
    create_input_box(setting_apps_change_char_buffer);
    change_setting_apps_id = id;
    waiting_change_setting_apps = true;
}

bool waiting_change_setting_background = false;

void change_setting_background()
{
    memset(setting_apps_change_char_buffer, 0, sizeof(setting_apps_change_char_buffer));
    char *background = get_background_file_path();
    if (background != NULL)
    {
        strcpy(setting_apps_change_char_buffer, background);
    }
    create_input_box(setting_apps_change_char_buffer);
    waiting_change_setting_background = true;
}

int read_settings_time_offset()
{
    int ret = 0;
    UserInfo usif;
    xapi_GetCurrentUser(&usif);
    char runfile_path[256];
    memset((void *)runfile_path, 0, 256);
    strcat(runfile_path, "/users/");
    strcat(runfile_path, usif.name);
    strcat(runfile_path, "/settings.dat");
    XFILE *f = xapi_OpenFile(runfile_path);
    if (!f) return 0;
    SettingsDataFileFormat *sdff = (SettingsDataFileFormat *)f->buffer;
    ret = sdff->ClockHourOffset;

    xapi_CloseFile(f);

    return ret;
}

void change_clock_hour_offset(int value)
{
    UserInfo usif;
    xapi_GetCurrentUser(&usif);
    char runfile_path[256];
    memset((void *)runfile_path, 0, 256);
    strcat(runfile_path, "/users/");
    strcat(runfile_path, usif.name);
    strcat(runfile_path, "/settings.dat");
    XFILE *f = xapi_OpenFile(runfile_path);
    if (!f) return;
    SettingsDataFileFormat *sdff = (SettingsDataFileFormat *)f->buffer;
    sdff->ClockHourOffset += value;

    xapi_CloseFile(f);

    xapi_FlushTime();
    draw_background_body();
}

char background_file_path[256];

void set_background_file_path(const char *path)
{
    if (path == NULL) return;

    UserInfo usif;
    xapi_GetCurrentUser(&usif);
    char settings_path[256];
    memset(settings_path, 0, sizeof(settings_path));
    snprintf(settings_path, sizeof(settings_path), "/users/%s/settings.dat", usif.name);
    XFILE *file = xapi_OpenFile(settings_path);
    if (file == NULL) return;

    SettingsDataFileFormat *settings = (SettingsDataFileFormat *)file->buffer;
    strncpy(settings->BackgroundFilePath, path, sizeof(settings->BackgroundFilePath) - 1);
    settings->BackgroundFilePath[sizeof(settings->BackgroundFilePath) - 1] = '\0';
    xapi_CloseFile(file);
    draw_background_body();
}

char *get_background_file_path()
{
    UserInfo usif;
    xapi_GetCurrentUser(&usif);
    char runfile_path[256];
    memset((void *)runfile_path, 0, 256);
    strcat(runfile_path, "/users/");
    strcat(runfile_path, usif.name);
    strcat(runfile_path, "/settings.dat");
    XFILE *f = xapi_OpenFile(runfile_path);
    if (!f) return NULL;
    SettingsDataFileFormat *sdff = (SettingsDataFileFormat *)f->buffer;
    strcpy(background_file_path, sdff->BackgroundFilePath);

    xapi_CloseFile(f);

    return background_file_path;
}
