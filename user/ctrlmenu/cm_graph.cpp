#include <x3api.h>
#include <libsys.h>
#include <krlibc.h>
#include "cm_proto.h"
#include "../../kernel/build_settings.h"

int cindex = 1;

bool dont_refresh = false;
bool about_memory_show_mb = false;

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

static int ctrlmenu_read_text_file(const char *path, char *buffer, size_t size)
{
    if (path == NULL || buffer == NULL || size == 0) return -1;

    int fd = (int)enter_syscall(SYS_OPEN, (uint64_t)path, 0, 0, 0, 0, 0);
    if (fd < 0) return -1;

    int got = read(fd, buffer, size - 1);
    close(fd);
    if (got < 0) return -1;

    buffer[got] = '\0';
    return got;
}

static void ctrlmenu_draw_network_status()
{
    static char nm_status_path[] = "/run/NetworkManager/status";
    char        status_text[512];
    memset(status_text, 0, sizeof(status_text));

    xapi_DrawText(handle, win_width / 10 * 3 + 24, 128, "NetworkManager", 16, 0xffffffff);
    xapi_DrawText(handle, win_width / 10 * 3 + 24, 156, "/run/NetworkManager/status", 11, 0xffc8f7ff);

    int got = ctrlmenu_read_text_file(nm_status_path, status_text, sizeof(status_text));
    if (got <= 0)
    {
        xapi_DrawText(handle, win_width / 10 * 3 + 24, 196, "网络运行态不可用。", 11, 0xffffffff);
        return;
    }

    int   y          = 196;
    char *cursor     = status_text;
    char  line[160];
    while (*cursor != '\0' && y < (int)win_height - 24)
    {
        size_t len = 0;
        while (cursor[len] != '\0' && cursor[len] != '\n' && len + 1 < sizeof(line))
        {
            line[len] = cursor[len];
            len++;
        }
        line[len] = '\0';

        if (cursor[len] == '\n') cursor += len + 1;
        else cursor += len;

        if (line[0] == '\0') continue;

        uint32_t color = 0xffffffff;
        char     draw_line[160];
        memset(draw_line, 0, sizeof(draw_line));

        if (line[0] == '[')
        {
            color = 0x00a2e8ff;
            size_t line_len = strlen(line);
            if (line_len >= 2 && line[line_len - 1] == ']')
            {
                strncpy(draw_line, line + 1, MIN(line_len - 2, sizeof(draw_line) - 1));
            }
            else
            {
                strncpy(draw_line, line, sizeof(draw_line) - 1);
            }
        }
        else
        {
            strncpy(draw_line, line, sizeof(draw_line) - 1);
        }

        xapi_DrawText(handle, win_width / 10 * 3 + 24, y, draw_line, 11, color);
        y += 24;
    }
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
    xapi_DrawPicture(handle, 0, 0, win_width, win_height, "/system/image/background3.png");
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
    xapi_DrawRect(handle, 8, win_height - 38, win_width - 8, win_height - 8, 0x00a2e8ff, true);
    xapi_DrawText(handle, 16, win_height - 38, "→ 返回桌面", 16, 0xffffffff);
}

int setting_cindex = 1;

void draw_setting()
{
    xapi_DrawRect(handle, win_width / 10 * 3 + 8, 128, win_width / 10 * 3 + 12, win_height - 8, 0x00a2e8ff, true);

    xapi_DrawRect(handle, 8, 128, win_width / 10 * 3, 158, 0x00a2e8ff, true);
    xapi_DrawText(handle, 16, 128, "系统", 16, 0xffffffff);

    xapi_DrawRect(handle, 8, 166, win_width / 10 * 3, 196, 0x00a2e8ff, true);
    xapi_DrawText(handle, 16, 166, "网络", 16, 0xffffffff);

    xapi_DrawRect(handle, 8, 204, win_width / 10 * 3, 234, 0x00a2e8ff, true);
    xapi_DrawText(handle, 16, 204, "外观", 16, 0xffffffff);

    xapi_DrawRect(handle, 8, 242, win_width / 10 * 3, 272, 0x00a2e8ff, true);
    xapi_DrawText(handle, 16, 242, "应用", 16, 0xffffffff);

    xapi_DrawRect(handle, 8, 280, win_width / 10 * 3, 310, 0x00a2e8ff, true);
    xapi_DrawText(handle, 16, 280, "时间与位置", 16, 0xffffffff);

    if (setting_cindex == 1)
    {
        xapi_DrawRect(handle, win_width / 10 * 3, 128, win_width / 10 * 3 + 8, 158, 0x00a2e8ff, true);

        char system_version[64];
        xapi_GetSystemVersion(system_version);
        char cpu_model[64];
        xapi_GetCpuModel(cpu_model);
        uint64_t memory_kib = xapi_GetMemorySize();
        char memory_size[64];
        memset(memory_size, 0, sizeof(memory_size));
        if (about_memory_show_mb)
        {
            strcat(memory_size, xcr_int2char(memory_kib / 1024));
            strcat(memory_size, " MB");
        }
        else
        {
            strcat(memory_size, xcr_int2char(memory_kib / 1024 / 1024));
            strcat(memory_size, " GB");
        }
        char fenbianlv[64];
        memset(fenbianlv, 0, sizeof(fenbianlv));
        strcat(fenbianlv, xcr_int2char(win_width));
        strcat(fenbianlv, "x");
        strcat(fenbianlv, xcr_int2char(win_height));

        xapi_DrawPicture(handle, win_width / 10 * 3 + 8, 129, 302, 170, "/system/xj380_2.png");

        xapi_DrawText(handle, win_width / 10 * 3 + 302, 128, "XJ380 操作系统", 16, 0xffffffff);
        xapi_DrawText(handle, win_width / 10 * 3 + 302, 158, OS_EDITION, 11, 0xffffffff);

        xapi_DrawText(handle, win_width / 10 * 3 + 302, 218, "处理器", 11, 0xffffffff);
        xapi_DrawText(handle, win_width / 10 * 3 + 402, 218, cpu_model, 11, 0xffffffff);

        xapi_DrawText(handle, win_width / 10 * 3 + 302, 248, "运行内存", 11, 0xffffffff);
        xapi_DrawText(handle, win_width / 10 * 3 + 402, 248, memory_size, 11, 0xffc8f7ff);

        xapi_DrawText(handle, win_width / 10 * 3 + 302, 278, "内核版本", 11, 0xffffffff);
        xapi_DrawText(handle, win_width / 10 * 3 + 402, 278, KN_VERSION, 11, 0xffffffff);

        xapi_DrawText(handle, win_width / 10 * 3 + 302, 308, "系统版本", 11, 0xffffffff);
        xapi_DrawText(handle, win_width / 10 * 3 + 402, 308, system_version, 11, 0xffffffff);

        xapi_DrawText(handle, win_width / 10 * 3 + 302, 338, "系统类型", 11, 0xffffffff);
        xapi_DrawText(handle, win_width / 10 * 3 + 402, 338, "x64 操作系统，基于 x86_64 处理器", 11, 0xffffffff);

        xapi_DrawText(handle, win_width / 10 * 3 + 302, 368, "分辨率", 11, 0xffffffff);
        xapi_DrawText(handle, win_width / 10 * 3 + 402, 368, fenbianlv, 11, 0xffffffff);
    }
    else if (setting_cindex == 2)
    {
        xapi_DrawRect(handle, win_width / 10 * 3, 166, win_width / 10 * 3 + 8, 196, 0x00a2e8ff, true);
        ctrlmenu_draw_network_status();
    }
    else if (setting_cindex == 3)
    {
        xapi_DrawRect(handle, win_width / 10 * 3, 204, win_width / 10 * 3 + 8, 234, 0x00a2e8ff, true);

        xapi_DrawText(handle, win_width / 10 * 3 + 24, 128, "外观", 16, 0xffffffff);

        xapi_DrawText(handle, win_width / 10 * 3 + 24, 168, "壁纸", 11, 0xffffffff);
        xapi_DrawText(handle, win_width / 10 * 3 + 124, 168, get_background_file_path(), 11, 0xffffffff);
        xapi_Button(handle, 3000, win_width - 100, 168, "更改");
    }
    else if (setting_cindex == 4)
    {
        xapi_DrawRect(handle, win_width / 10 * 3, 242, win_width / 10 * 3 + 8, 272, 0x00a2e8ff, true);
        draw_setting_apps();
    }
    else if (setting_cindex == 5)
    {
        xapi_DrawRect(handle, win_width / 10 * 3, 280, win_width / 10 * 3 + 8, 310, 0x00a2e8ff, true);
        
        char current_time_title[256];
        TimeType tm;
        xapi_GetTimeX(&tm);
        sprintf(current_time_title, "当前时间：%d 年 %d 月 %d 日 %02u : %02u", tm.tm_year, tm.tm_mon, tm.tm_mday, tm.tm_hour, tm.tm_min);
        xapi_DrawText(handle, win_width / 10 * 3 + 24, 128, current_time_title, 16, 0xffffffff);
        xapi_DrawText(handle, win_width / 10 * 3 + 24, 158, "当前偏移：", 11, 0xffffffff);
        xapi_DrawText(handle, win_width / 10 * 3 + 85, 158, xcr_int2char(read_settings_time_offset()), 11, 0xffffffff);
        xapi_Button(handle, 2001, win_width / 10 * 3 + 400, 132, "调整 (+1)");
        xapi_Button(handle, 2002, win_width / 10 * 3 + 490, 132, "调整 (-1)");
    }
}

char setting_apps_change_char_buffer[256];

void draw_setting_apps()
{
    UserInfo usif;
    xapi_GetCurrentUser(&usif);
    char runfile_path[256];
    memset((void *)runfile_path, 0, 256);
    strcat(runfile_path, "/users/");
    strcat(runfile_path, usif.name);
    strcat(runfile_path, "/runfile.dat");
    XFILE *f = xapi_OpenFile(runfile_path);
    if (!f) return;
    RunfileSettings_Format *rff = (RunfileSettings_Format *)f->buffer;
    xapi_DrawText(handle, win_width / 10 * 3 + 24, 128, "默认应用", 16, 0xffffffff);

    for (int i = 0; rff->items[i].exname[0] != '\0'; i++)
    {
        char temp_exname[11] = ".";
        strcat(temp_exname, rff->items[i].exname);
        xapi_DrawText(handle, win_width / 10 * 3 + 24 , 168 + i * 32, temp_exname, 11, 0xffffffff);
        xapi_DrawText(handle, win_width / 10 * 3 + 124, 168 + i * 32, rff->items[i].describe, 11, 0xffffffff);
        xapi_DrawText(handle, win_width / 10 * 3 + 274, 168 + i * 32, rff->items[i].runpath, 11, 0xffffffff);
        xapi_Button(handle, i, win_width - 100, 168 + i * 32, "更改");
    }

    xapi_CloseFile(f);
}

int charbox_count = 0;
char *charbox_buf = NULL;
int charbox_input_complete = 0;

int change_setting_apps_id = -1;

void create_input_box(char *buf)
{
    dont_refresh = true;
    xapi_DrawRect(handle, win_width / 2 - 300, win_height / 2 - 50, win_width / 2 + 300, win_height / 2 + 50, 0x007accff, true);
    xapi_DrawText(handle, win_width / 2 - 300 + 8, win_height / 2 - 50 + 4, "输入新的值：", 11, 0xffffffff);
    xapi_DrawRect(handle, win_width / 2 - 300 + 8, win_height / 2 - 10, win_width / 2 + 300 - 8, win_height / 2 + 10, 0xffffffff, true);
    xapi_Button(handle, 114514, win_width / 2 + 300 - 58, win_height / 2 + 50 - 32, "取消");
    xapi_Button(handle, 114515, win_width / 2 + 300 - 116, win_height / 2 + 50 - 32, "确定");
    xapi_RefreshPartWindow(handle, win_width / 2 - 300, win_height / 2 - 50, win_width / 2 + 300, win_height / 2 + 50);

    charbox_count = 0;
    charbox_buf = buf;
}

void put_char(char ch)
{
    if (!charbox_buf) return;

    char temp_char_buf[2] = {ch, '\0'};
    xapi_DrawSWText(handle, win_width / 2 - 300 + 12 + charbox_count * 9, win_height / 2 - 10, temp_char_buf, 0x000000ff);
    charbox_buf[charbox_count] = ch;
    charbox_count++;
}

void backspace()
{
    if (charbox_count <= 0) return;

    charbox_count--;
    charbox_buf[charbox_count] = '\0';
    xapi_DrawRect(handle, 
        win_width / 2 - 300 + 12 + charbox_count * 9, 
        win_height / 2 - 10, 
        win_width / 2 - 300 + 12 + charbox_count * 9 + 9, 
        win_height / 2 + 10, 0xffffffff, true);
    xapi_RefreshPartWindow(handle, 
        win_width / 2 - 300 + 12 + charbox_count * 9, 
        win_height / 2 - 10, 
        win_width / 2 - 300 + 12 + charbox_count * 9 + 9, 
        win_height / 2 + 10);
}

void delete_input_box()
{
    charbox_buf[charbox_count] = '\0';
    charbox_input_complete = true;
    dont_refresh = false;
    draw_background_body();
}

bool waiting_change_setting_apps = false;

void change_setting_apps(int id)
{
    create_input_box(setting_apps_change_char_buffer);
    change_setting_apps_id = id;
    waiting_change_setting_apps = true;
}

bool waiting_change_setting_background = false;

void change_setting_background()
{
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
