#include "./image/stbi.h"
#include "./image/stbir.h"
#include <ahci/ahci.h>
#include <cpu/regio.h>
#include <device.h>
#include <efi/boot.h>
#include <efi/efi.h>
#include <efi/fbc.h>
#include <font.h>
#include <fs/fatfs/fatfs.h>
#include <fs/partition.h>
#include <fs/vfs/devfs.h>
#include <fs/vfs/proc.h>
#include <fs/vfs/sys.h>
#include <fs/vfs/vfs.h>
#include <global_color.h>
#include <graphics/GOP.hpp>
#include <graphics/sheet.h>
#include <graphics/window/window.h>
#include <hda/hda.h>
#include <hda/pcspk.h>
#include <krlibc.h>
#include <mm/frame.h>
#include <nvme/nvme.h>
#include <pci/pci.h>
#include <pipe.h>
#include <proto.hpp>
#include <ps2/keyboard.h>
#include <ps2/mouse.h>
#include <rtc.h>
#include <sb16.h>
#include <syscall/signal.h>
#include <syscall/syscall.h>
#include <task/pcb.h>
#include <ttf.h>
#include <user/runfile.h>

extern UserInfo *current_user;

// 初始化桌面
void init_desktop(SHEET_INFO *sht, SHEET *csheet, SHEET *dock_ct_sheet)
{
    int           scrx = sht->scrx;
    int           scry = sht->scry;
    // #136dff
    // draw_rect(sht, ct_sheet, 0, 0, scrx - 1, scry - 1, WIN_BLUE);

    // 获取路径
    char runfile_setfile_path[256];
    memset(runfile_setfile_path, 0, 256);
    strcat(runfile_setfile_path, "/Users/");
    strcat(runfile_setfile_path, current_user->name);
    strcat(runfile_setfile_path, "/settings.dat");
    vfs_mkfile(runfile_setfile_path);

    char  settings_file_buffer[sizeof(SettingsDataFileFormat)];
    vfs_node_t settings_file_v = vfs_open(runfile_setfile_path);
    if (!settings_file_v) return;
    vfs_read(settings_file_v, settings_file_buffer, 0, sizeof(SettingsDataFileFormat));

    SettingsDataFileFormat *file_format = (SettingsDataFileFormat *)settings_file_buffer;

    // 渲染
    SHEET_BUFFER *buff = (SHEET_BUFFER *)malloc(scrx * scry * 4);
    int           w, h, bpp;
    vfs_node_t    v = vfs_open(file_format->BackgroundFilePath);
    if (!v)
    {
        write_serial_fmt("init_desktop: cannot open '%s'\n", file_format->BackgroundFilePath);
        return;
    }
    unsigned char *bf = (unsigned char *)malloc(v->size);
    if (!bf)
    {
        write_serial_fmt("init_desktop: failed to allocate %u bytes for '%s'\n", (unsigned)v->size,
                         file_format->BackgroundFilePath);
        vfs_close(v);
        return;
    }
    vfs_read(v, (uint8_t *)bf, 0, v->size);
    stbi_uc *b = stbi_load_from_memory(bf, v->size, &w, &h, &bpp, 0);
    free(bf);
    if (!b)
    {
        write_serial_fmt("init_desktop: decode failed for '%s' (%u bytes)\n", file_format->BackgroundFilePath,
                         (unsigned)v->size);
        vfs_close(v);
        return;
    }
    stbir_resize_uint8(b, w, h, 0, (uint8_t *)(buff), scrx, scry, 0, 4);
    stbi_image_free(b);
    vfs_close(v);

    SHEET *front_p = sht->start;
    while (true)
    {
        if (front_p == csheet) { break; }
        else if (front_p->next == NULL) { return; }
        front_p = front_p->next;
    }

    // SHEET_BUFFER *SheetBuffer = (SHEET_BUFFER *)(front_p->buffer);

    // 检查内存对齐情况
    front_p->buffer = (void *)buff;

    init_dock(sht, dock_ct_sheet);
    init_shortcut_dock(sht, dock_ct_sheet);

    char desktop_folder_path[256] = "/Users/";
    strcat(desktop_folder_path, current_user->name);
    strcat(desktop_folder_path, "/desktop");
    vfs_mkdir(desktop_folder_path);

    vfs_node_t desktop_setting_file_v = vfs_open(desktop_folder_path);
    if (!desktop_setting_file_v) return;

    strcat(desktop_folder_path, "/testfolder");
    vfs_mkdir(desktop_folder_path);

    PrintPicture_blend(sht, csheet, 20, 30 + 0 * 110, 74, 74, "/system/icon/d_system.png");
    print_box_ttf(sht, csheet, "系统", WHITE, 57 - calc_ttf_length("系统", 11) / 2, 105 + 0 * 110, 11);

    PrintPicture_blend(sht, csheet, 20, 30 + 1 * 110, 74, 74, "/system/icon/bin.png");
    print_box_ttf(sht, csheet, "垃圾桶", WHITE, 57 - calc_ttf_length("垃圾桶", 11) / 2, 105 + 1 * 110, 11);

    // 每个格是 110 * 110，从（0，25）开始
    int nindex = 2;
    for (list_t node = desktop_setting_file_v->child; node != NULL; node = node->next)
    {
        vfs_node_t data = (vfs_node_t)node->data;
        int bx = 20 + 110 * (nindex / ((sht->scry - 88) / 110));
        int by = 30 + (nindex - (nindex / ((sht->scry - 88) / 110)) * ((sht->scry - 88) / 110)) * 110;
        if (data->type == file_dir)
        {
            // 是文件夹
            PrintPicture_blend(sht, csheet, bx, by, 74, 74, "/system/icon/folder.png");
        }
        else
        {
            // 是文件
            PrintPicture_blend(sht, csheet, bx, by, 74, 74, "/system/icon/texter.png");
        }
        print_box_ttf(sht, csheet, 
            data->name, WHITE, 
            57 - calc_ttf_length(data->name, 11) / 2, 105 + nindex * 110, 11);
        nindex++;
    }

    vfs_close(desktop_setting_file_v);
}

void init_dock(SHEET_INFO *sht, SHEET *csheet)
{
    int scrx = sht->scrx;
    int scry = sht->scry;
    int tmpx = scrx * 0.75; // 75%
    int tmpy = scry - 48;
    draw_rect(sht, csheet, 0, scry - 48, tmpx - 1, scry - 1, DOCK_COL);

    for (int y = 0; y <= 47; y++)
    {
        for (int x = 0; x <= y; x++)
        {
            draw_point(sht, csheet, x + tmpx, y + tmpy, DOCK_COL);
        }
    }

    draw_logo(sht, csheet, 24, scry - 24 - 63);
}

void init_shortcut_dock(SHEET_INFO *sht, SHEET *csheet)
{
    int x1 = sht->scrx * 0.1875;
    int y1 = 0;
    int x2 = sht->scrx * 0.8125;
    int y2 = 24;
    draw_rect(sht, csheet, x1, y1, x2 - 1, y2 - 1, DOCK_COL);

    draw_studio_logo(sht, csheet, x1 + 8, 4);

    print_box_ttf(sht, csheet, (char *)"文件", BLACK, x1 + 32, 1, 10);
    print_box_ttf(sht, csheet, (char *)"设置", BLACK, x1 + 32 + 25 + 8, 1, 10);
    print_box_ttf(sht, csheet, (char *)"终端", BLACK, x1 + 32 + 25 + 8 + 25 + 9, 1, 10);
    print_box_ttf(sht, csheet, (char *)"图形", BLACK, x1 + 32 + 25 + 8 + 25 + 9 + 25 + 9, 1, 10);
    print_box_ttf(sht, csheet, (char *)"关于", BLACK, x1 + 32 + 25 + 8 + 25 + 9 + 25 + 9 + 25 + 8, 1, 10);
    // PrintString(sht, ct_sheet, x1 + 32, 4, "Files", BLACK);
    // PrintString(sht, ct_sheet, x1 + 32 + 40 + 8, 4, "Settings", BLACK);
    // PrintString(sht, ct_sheet, x1 + 32 + 40 + 8 + 64 + 8, 4, "Terminal", BLACK);
    // PrintString(sht, ct_sheet, x1 + 32 + 40 + 8 + 64 + 8 + 64 + 8, 4, "Graphics", BLACK);
    // PrintString(sht, ct_sheet, x1 + 32 + 40 + 8 + 64 + 8 + 64 + 8 + 64 + 8, 4, "About", BLACK);
    tm time;
    time_read(&time);
    draw_rect(sht, csheet, x2 - 40, 0, x2 - 3, 23, DOCK_COL);
    print_fmt_box_ttf(sht, csheet, BLACK, x2 - 40, 1, 10, "%02u:%02u", time.tm_hour % 24, time.tm_min);
}

void process_desktop_find_block(int x, int y)
{
    char desktop_folder_path[265] = "/Users/";
    memset(desktop_folder_path, 0, 256);    // 回收再利用
    strcat(desktop_folder_path, "/Users/");
    strcat(desktop_folder_path, current_user->name);
    strcat(desktop_folder_path, "/desktop");

    vfs_node_t desktop_setting_file_v = vfs_open(desktop_folder_path);
    if (!desktop_setting_file_v) return;

    // 每个格是 110 * 110，从（0，25）开始
    int i;
    for (i = 0; i < 2; i++)
    {
        int bx = 20 + 110 * (i / ((sht_img->scry - 88) / 110));
        int by = 30 + (i - (i / ((sht_img->scry - 88) / 110)) * ((sht_img->scry - 88) / 110)) * 110;

        if (x > bx && x < bx + 110 &&
            y > by && y < by + 110)
        {
            // 就是他！
            if (i == 0)
            {
                create_user_process_from_file((char *)"/system/fmanager.elf", NULL, NULL);
            }
        }
    }
    for (list_t node = desktop_setting_file_v->child; node != NULL; node = node->next)
    {
        int bx = 20 + 110 * (i / ((sht_img->scry - 88) / 110));
        int by = 30 + (i - (i / ((sht_img->scry - 88) / 110)) * ((sht_img->scry - 88) / 110)) * 110;

        if (x > bx && x < bx + 110 &&
            y > by && y < by + 110)
        {
            // 就是他！
            vfs_node_t data = (vfs_node_t)node->data;
            char runfile_path[256] = "\0";
            strcat(runfile_path, "/Users/");
            strcat(runfile_path, current_user->name);
            strcat(runfile_path, "/desktop/");
            strcat(runfile_path, data->name);
            if (data->type == file_dir)
            {
                // 是文件夹
                char *argv[2] = {runfile_path, NULL};
                create_user_process_from_file((char *)"/system/fmanager.elf", NULL, argv);
            }
            else
            {
                runfile(runfile_path);
            }
        }
        i++;
    }

    vfs_close(desktop_setting_file_v);
}

SHEET_BUFFER LCD_AlphaBlend(SHEET_BUFFER foreground_color, SHEET_BUFFER background_color, uint8_t alpha)
{
    uint8_t       text_r   = foreground_color.r;
    uint8_t       text_g   = foreground_color.g;
    uint8_t       text_b   = foreground_color.b;
    uint8_t       text_a   = foreground_color.a;
    uint8_t       coverage = alpha;
    SHEET_BUFFER *dst      = &background_color;

    // 转换为浮点运算（0.0-1.0范围）
    float cov       = coverage / 255.0f;
    float txt_alpha = text_a / 255.0f;
    float src_alpha = cov * txt_alpha;

    // 源颜色预乘alpha
    float src_r = (text_r / 255.0f) * src_alpha;
    float src_g = (text_g / 255.0f) * src_alpha;
    float src_b = (text_b / 255.0f) * src_alpha;

    // 目标颜色预乘alpha
    float dst_a = dst->a / 255.0f;
    float dst_r = (dst->r / 255.0f) * dst_a;
    float dst_g = (dst->g / 255.0f) * dst_a;
    float dst_b = (dst->b / 255.0f) * dst_a;

    // 混合计算
    float blended_alpha = src_alpha + dst_a * (1.0f - src_alpha);

    // 颜色混合
    float blended_r = (src_r + dst_r * (1.0f - src_alpha)) / blended_alpha;
    float blended_g = (src_g + dst_g * (1.0f - src_alpha)) / blended_alpha;
    float blended_b = (src_b + dst_b * (1.0f - src_alpha)) / blended_alpha;

    // 转换回0-255并存储
    dst->r = (uint8_t)(blended_r * 255 + 0.5f);
    dst->g = (uint8_t)(blended_g * 255 + 0.5f);
    dst->b = (uint8_t)(blended_b * 255 + 0.5f);
    dst->a = (uint8_t)(blended_alpha * 255 + 0.5f);
    return *dst;
}

void draw_logo(SHEET_INFO *sht, SHEET *csheet, int xi, int yi)
{ PrintPicture_blend(sht, csheet, xi, yi, 84, 64, "/system/xj380.png"); }

void draw_studio_logo(SHEET_INFO *sht, SHEET *csheet, int xi, int yi)
{ PrintPicture_blend(sht, csheet, xi, yi, 16, 16, "/system/xingji.png"); }

TASK_DOCK_BLOCK tmp_dock_block   = {.mcount = 0, .next = NULL};
TDB_t           first_dock_block = &tmp_dock_block;

int task_dock_index_bx = 132;

extern SHEET *dock_ct_sheet;

char out_focus_window_dock_ball_bitmap[4][5] = {
    ".##.",
    "####",
    "####",
    ".##.",
};

char in_focus_window_dock_ball_bitmap[4][17] = {
    ".##############.",
    "################",
    "################",
    ".##############.",
};

spin_t task_dock_lock = SPIN_INIT;

void paint_task_dock_item(TDB_t tdb, int count)
{
    draw_rect(sht_img, dock_ct_sheet, task_dock_index_bx + 80 * count, sht_img->scry - 48,
              task_dock_index_bx + 80 * count + 64, sht_img->scry - 1, DOCK_COL);
    draw_rect(sht_img, dock_ct_sheet, task_dock_index_bx + 80 * count, sht_img->scry - 24 - 63,
              task_dock_index_bx + 80 * count + 64, sht_img->scry - 49, {0, 0, 0, 0});

    PrintPicture_blend(sht_img, dock_ct_sheet, task_dock_index_bx + 80 * count, sht_img->scry - 24 - 63, 64, 64,
                       tdb->path);

    if (tdb->in_focus)
    {
        for (int y = 0; y < 4; y++)
        {
            for (int x = 0; x < 16; x++)
            {
                if (in_focus_window_dock_ball_bitmap[y][x] == '#')
                {
                    draw_point(sht_img, dock_ct_sheet, x + task_dock_index_bx + 80 * count + 24,
                               y + sht_img->scry - 16, WIN_BLUE);
                }
            }
        }
    }
    else
    {
        for (int y = 0; y < 4; y++)
        {
            for (int x = 0; x < 4; x++)
            {
                if (out_focus_window_dock_ball_bitmap[y][x] == '#')
                {
                    draw_point(sht_img, dock_ct_sheet, x + task_dock_index_bx + 80 * count + 30,
                               y + sht_img->scry - 16, BGRAY);
                }
            }
        }
    }

    refresh_part_sheet(sht_img, task_dock_index_bx + 80 * count, sht_img->scry - 24 - 63,
                       task_dock_index_bx + 80 * count + 64, sht_img->scry);
}

extern bool have_full_screen_app;

void flush_task_dock()
{
    if (have_full_screen_app) return;

    spin_lock(task_dock_lock);
    TDB_t front_p = first_dock_block;
    int   i       = 0;
    while (true)
    {
        if (front_p->mcount)
        {
            paint_task_dock_item(front_p, i);
            i++;
        }
        if (front_p->next == NULL) { break; }
        front_p = front_p->next;
    }
    spin_unlock(task_dock_lock);
}

void register_task_dock(WINDOWLSP window)
{
    spin_lock(task_dock_lock);
    TDB_t front_p = first_dock_block;
    TDB_t new_win = (TDB_t)malloc(sizeof(TASK_DOCK_BLOCK));
    memset(new_win, 0, sizeof(TASK_DOCK_BLOCK));
    strcpy(new_win->path, "/system/icon/unknowexe.png");
    new_win->mcount   = 1;
    new_win->windowls = window;
    new_win->next     = NULL;
    new_win->in_focus = false;
    new_win->min_mode = false;

    front_p = first_dock_block;
    while (true)
    {
        if (front_p->next == NULL)
        {
            front_p->next = new_win;
            break;
        }
        front_p = front_p->next;
    }

    spin_unlock(task_dock_lock);
    flush_task_dock();
}

void unregister_task_dock(WINDOWLSP window)
{
    spin_lock(task_dock_lock);
    int   i       = 0;
    TDB_t front_p = first_dock_block;
    TDB_t front_p2;
    while (true)
    {
        if (front_p->windowls == window)
        {
            front_p2->next = front_p->next;
            free(front_p);

            if (have_full_screen_app) break;

            int count = i;
            draw_rect(sht_img, dock_ct_sheet, task_dock_index_bx + 80 * count, sht_img->scry - 48,
                      task_dock_index_bx + 80 * count + 64, sht_img->scry - 1, DOCK_COL);
            draw_rect(sht_img, dock_ct_sheet, task_dock_index_bx + 80 * count, sht_img->scry - 24 - 63,
                      task_dock_index_bx + 80 * count + 64, sht_img->scry - 49, {0, 0, 0, 0});
            refresh_part_sheet(sht_img, task_dock_index_bx + 80 * count, sht_img->scry - 24 - 63,
                               task_dock_index_bx + 80 * count + 64, sht_img->scry);

            if (front_p2->next == NULL) break;

            count++;
            draw_rect(sht_img, dock_ct_sheet, task_dock_index_bx + 80 * count, sht_img->scry - 48,
                      task_dock_index_bx + 80 * count + 64, sht_img->scry - 1, DOCK_COL);
            draw_rect(sht_img, dock_ct_sheet, task_dock_index_bx + 80 * count, sht_img->scry - 24 - 63,
                      task_dock_index_bx + 80 * count + 64, sht_img->scry - 49, {0, 0, 0, 0});
            refresh_part_sheet(sht_img, task_dock_index_bx + 80 * count, sht_img->scry - 24 - 63,
                               task_dock_index_bx + 80 * count + 64, sht_img->scry);

            break;
        }
        if (front_p->next == NULL) { break; }
        if (front_p->mcount) { i++; }
        front_p2 = front_p;
        front_p  = front_p->next;
    }

    spin_unlock(task_dock_lock);
    flush_task_dock();
}

void focus_window_dock(WINDOWLSP win)
{
    spin_lock(task_dock_lock);
    TDB_t front_p = first_dock_block;
    while (true)
    {
        front_p->in_focus = false;
        if (front_p->windowls == win) { front_p->in_focus = true; }
        if (front_p->next == NULL) { break; }
        front_p = front_p->next;
    }

    spin_unlock(task_dock_lock);
    flush_task_dock();
}

void change_task_dock_icon(WINDOWLSP win, char *path)
{
    spin_lock(task_dock_lock);
    TDB_t front_p = first_dock_block;
    while (true)
    {
        front_p->in_focus = false;
        if (front_p->windowls == win)
        {
            memset(front_p->path, 0, 64);
            strcpy(front_p->path, path);
        }
        if (front_p->next == NULL) { break; }
        front_p = front_p->next;
    }

    spin_unlock(task_dock_lock);
    flush_task_dock();
}

void save_window_xy(WINDOWLSP win)
{
    spin_lock(task_dock_lock);
    TDB_t front_p = first_dock_block;
    while (true)
    {
        front_p->in_focus = false;
        if (front_p->windowls == win)
        {
            front_p->bmx = win->w_sheet->bx;
            front_p->bmy = win->w_sheet->by;
            front_p->in_focus = false;
            front_p->min_mode = true;
        }
        if (front_p->next == NULL) { break; }
        front_p = front_p->next;
    }

    spin_unlock(task_dock_lock);
    flush_task_dock();
}

TDB_t find_dock_icon(int index)
{
    TDB_t front_p = first_dock_block;
    for (int i = 0; i < index + 1; i++)
    {
        if (front_p->next == NULL) { return NULL; }
        front_p = front_p->next;
    }

    return front_p;
}

void draw_app_message_box(char *title, char *text)
{
    if (have_full_screen_app) return;

    int scdx = fbc_addr->horizontal_resolution * 13 / 16 - 1;
    draw_rect(sht_img, dock_ct_sheet, scdx - 300, 28, scdx, 128, DOCK_COL);

    print_box_ttf(sht_img, dock_ct_sheet, title, BLACK, scdx - 300 + 8, 28, 16);
    print_box_ttf(sht_img, dock_ct_sheet, text, BLACK, scdx - 300 + 8, 60, 10);
    refresh_part_sheet(sht_img, scdx - 300, 28, scdx, 128);

    delay_s_hp(5);

    draw_rect(sht_img, dock_ct_sheet, scdx - 300, 28, scdx, 128, {0, 0, 0, 0});
    refresh_part_sheet(sht_img, scdx - 300, 28, scdx, 128);
}

bool logo_menu_just_born = false;

void draw_logo_menu()
{
    if (have_full_screen_app) return;
    int x1 = sht_img->scrx * 0.1875;
    draw_rect(sht_img, dock_ct_sheet, x1, 28, x1 + 99, 49, DOCK_COL);
    print_box_ttf(sht_img, dock_ct_sheet, "关机", {0,0,0,255}, x1 + 8, 28, 10);
    refresh_part_sheet(sht_img, x1, 28, x1 + 100, 50);
    logo_menu_just_born = true;
}

void delete_logo_menu()
{
    if (have_full_screen_app) { return; }
    if (logo_menu_just_born)
    {
        logo_menu_just_born = false;
        return;
    }

    int x1 = sht_img->scrx * 0.1875;
    draw_rect(sht_img, dock_ct_sheet, x1, 28, x1 + 99, 49, {0, 0, 0, 0});
    refresh_part_sheet(sht_img, x1, 28, x1 + 100, 50);
}
