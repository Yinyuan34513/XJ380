#pragma once

#include <stdint.h>

extern HDLE handle;

extern UINT64 win_width;
extern UINT64 win_height;

extern int cindex;
extern int setting_cindex;

extern bool exit_cm;

extern char setting_apps_change_char_buffer[256];
extern int charbox_input_complete;

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

void draw_background();
void process_left_key(int x, int y);
void draw_mainpage();
void draw_setting();
void create_input_box(char *buf);
void put_char(char ch);
void backspace();
void delete_input_box();

void draw_setting_apps();
void change_setting_apps(int id);

