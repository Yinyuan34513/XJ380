#pragma once

#include <stdint.h>

extern HDLE handle;

extern UINT64 win_width;
extern UINT64 win_height;

extern int cindex;
extern int setting_cindex;
extern bool about_memory_show_mb;

extern bool exit_cm;

extern char setting_apps_change_char_buffer[256];
extern int charbox_input_complete;

extern bool waiting_change_setting_apps;
extern bool waiting_change_setting_background;
extern int change_setting_apps_id;

struct RunfileSettings_Item
{
    char exname[10];
    char describe[128];  // 描述
    char runpath[256];  // 打开方式
};

struct RunfileSettings_Format
{
    RunfileSettings_Item items[1024];
};

struct SettingsDataFileFormat
{
    char    BackgroundFilePath[256];
    int     ClockHourOffset;
};

void draw_background();
void draw_background_body();
void draw_background_time();
void init_ctrlmenu_background_cache();
void process_left_key(int x, int y);
void draw_mainpage();
void draw_app_showcase();
void draw_setting();
bool ctrlmenu_handle_mainpage_click(int x, int y);
bool ctrlmenu_handle_app_showcase_click(int x, int y);
void ctrlmenu_settings_init();
void ctrlmenu_settings_hide_controls();
bool ctrlmenu_settings_handle_click(int x, int y);
bool ctrlmenu_settings_handle_control(UINT64 id, UINT64 data);
void ctrlmenu_settings_scroll(int delta);
void create_input_box(char *buf);
void delete_input_box(bool save);

void change_setting_apps(int id);

void change_setting_background();
void set_background_file_path(const char *path);

int read_settings_time_offset();
void change_clock_hour_offset(int value);
char *get_background_file_path();
