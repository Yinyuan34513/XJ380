#include <global_color.h>
#include <graphics/GOP.hpp>
#include <graphics/mouse.hpp>
#include <graphics/sheet.h>
#include <proto.hpp>
#include <ps2/keyboard.h>
#include <ps2/mouse.h>

mouse_dec ms_dec;
int       mouse_x = 0;
int       mouse_y = 0;
int       tby;
int       tbx;

extern char wbutton_close[18][33];
extern char wbutton_little[18][33];
bool        win_c_bt_isl = false;
bool        win_m_bt_isl = false;
#define WINDOW_ALPHA_VALUE 0xaa

extern "C" void c_mouse_handler(void *regs_ptr, uint64_t error_code)
{
    uint8_t data = inb(PORT_KB_DATA);

    if (mousedecode(data))
    {
        ms_dec.need_flush = true;

        mouse_x += ms_dec.x;
        mouse_y += ms_dec.y;
        if (mouse_x < 0) mouse_x = 0;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_x > get_hor() - 1) mouse_x = get_hor() - 1;
        if (mouse_y > get_ver() - 1) mouse_y = get_ver() - 1;

        ms_dec.left   = false;
        ms_dec.center = false;
        ms_dec.right  = false;

        if (ms_dec.btn & 0x01)
        {
            // 左键按下
            ms_dec.left = true;
        }
        else if ((ms_dec.btn & 0x01) == 0)
        {
            // 左键松开
            ms_dec.win_move_lock = false;
        }
        if (ms_dec.btn & 0x02) { ms_dec.right = true; }
        if (ms_dec.btn & 0x04) { ms_dec.center = true; }
    }

    send_eoi();
}

void process_mouse_info()
{
    if (ms_dec.sht_now != NULL && mouse_x >= ms_dec.sht_now->bx + ms_dec.sht_now->width - 32 &&
        mouse_x <= ms_dec.sht_now->bx + ms_dec.sht_now->width && mouse_y >= ms_dec.sht_now->by &&
        mouse_y <= ms_dec.sht_now->by + 17)
    {
        if (!win_c_bt_isl)
        {
            for (int y = 0; y < 17; y++)
            {
                for (int x = 0; x < 32; x++)
                {
                    if (wbutton_close[y][x] == '.')
                    {
                        draw_point(sht_img, ms_dec.sheet_id, x + ms_dec.sht_now->width - 32, y,
                                   {0xed, 0x1c, 0x24, 0xff});
                    }
                    else if (wbutton_close[y][x] == 'a')
                    {
                        draw_point(sht_img, ms_dec.sheet_id, x + ms_dec.sht_now->width - 32, y, {0, 0, 0, 0});
                    }
                    else if (wbutton_close[y][x] == '@')
                    {
                        draw_point(sht_img, ms_dec.sheet_id, x + ms_dec.sht_now->width - 32, y, {0, 0, 0, 0xff});
                    }
                }
            }
            refresh_part_sheet(sht_img, ms_dec.sht_now->bx + ms_dec.sht_now->width - 33, ms_dec.sht_now->by,
                               ms_dec.sht_now->bx + ms_dec.sht_now->width, ms_dec.sht_now->by + 17);
            win_c_bt_isl = true;
        }
    }
    else if (win_c_bt_isl)
    {
        for (int y = 0; y < 17; y++)
        {
            for (int x = 0; x < 32; x++)
            {
                if (wbutton_close[y][x] == '.')
                {
                    draw_point(sht_img, ms_dec.sheet_id, x + ms_dec.sht_now->width - 32, y,
                               {0xed, 0x1c, 0x24, WINDOW_ALPHA_VALUE});
                }
                else if (wbutton_close[y][x] == 'a')
                {
                    draw_point(sht_img, ms_dec.sheet_id, x + ms_dec.sht_now->width - 32, y, {0, 0, 0, 0});
                }
                else if (wbutton_close[y][x] == '@')
                {
                    draw_point(sht_img, ms_dec.sheet_id, x + ms_dec.sht_now->width - 32, y,
                               {0, 0, 0, WINDOW_ALPHA_VALUE});
                }
            }
        }
        refresh_part_sheet(sht_img, ms_dec.sht_now->bx + ms_dec.sht_now->width - 32, ms_dec.sht_now->by,
                           ms_dec.sht_now->bx + ms_dec.sht_now->width, ms_dec.sht_now->by + 17);
        win_c_bt_isl = false;
    }

    if (ms_dec.sht_now != NULL && mouse_x >= ms_dec.sht_now->bx + ms_dec.sht_now->width - 63 &&
        mouse_x <= ms_dec.sht_now->bx + ms_dec.sht_now->width - 32 && mouse_y >= ms_dec.sht_now->by &&
        mouse_y <= ms_dec.sht_now->by + 17)
    {
        if (!win_m_bt_isl)
        {
            for (int y = 0; y < 17; y++)
            {
                for (int x = 0; x < 32; x++)
                {
                    if (wbutton_little[y][x] == '@')
                    {
                        draw_point(sht_img, ms_dec.sheet_id, x + ms_dec.sht_now->width - 63, y, {0, 0, 0, 0xff});
                    }
                    else if (wbutton_little[y][x] == '.')
                    {
                        draw_point(sht_img, ms_dec.sheet_id, x + ms_dec.sht_now->width - 63, y,
                                   {0x00, 0xa2, 0xe8, 0xff});
                    }
                }
            }
            refresh_part_sheet(sht_img, ms_dec.sht_now->bx + ms_dec.sht_now->width - 63, ms_dec.sht_now->by,
                               ms_dec.sht_now->bx + ms_dec.sht_now->width - 32, ms_dec.sht_now->by + 17);
            win_m_bt_isl = true;
        }
    }
    else if (win_m_bt_isl)
    {
        for (int y = 0; y < 17; y++)
        {
            for (int x = 0; x < 32; x++)
            {
                if (wbutton_little[y][x] == '@')
                {
                    draw_point(sht_img, ms_dec.sheet_id, x + ms_dec.sht_now->width - 63, y,
                               {0, 0, 0, WINDOW_ALPHA_VALUE});
                }
                else if (wbutton_little[y][x] == '.')
                {
                    draw_point(sht_img, ms_dec.sheet_id, x + ms_dec.sht_now->width - 63, y,
                               {0x00, 0xa2, 0xe8, WINDOW_ALPHA_VALUE});
                }
            }
        }
        refresh_part_sheet(sht_img, ms_dec.sht_now->bx + ms_dec.sht_now->width - 63, ms_dec.sht_now->by,
                           ms_dec.sht_now->bx + ms_dec.sht_now->width - 32, ms_dec.sht_now->by + 17);
        win_m_bt_isl = false;
    }

    if (ms_dec.left)
    {
        if (!ms_dec.win_move_lock)
        {
            ms_dec.sheet_id = found_sheet_movable(sht_img, mouse_x, mouse_y);
            if (ms_dec.sheet_id == NULL)
            {
                // 点在了桌面或快捷栏上
                ms_dec.win_move_lock = false;
            }
            else
            {
                // 点在了窗口上
                ms_dec.win_move_lock = true;
                ms_dec.sht_now       = get_sheet(sht_img, ms_dec.sheet_id);
                lift_sheet(sht_img, ms_dec.sheet_id);
                refresh_part_sheet(sht_img, ms_dec.sht_now->bx, ms_dec.sht_now->by,
                                   ms_dec.sht_now->bx + ms_dec.sht_now->width,
                                   ms_dec.sht_now->by + ms_dec.sht_now->height);
                ms_dec.win_x_offset = mouse_x - ms_dec.sht_now->bx;
                ms_dec.win_y_offset = mouse_y - ms_dec.sht_now->by;
            }
            ms_dec.sheet_id = found_sheetmb(sht_img, mouse_x, mouse_y);
            if (ms_dec.sheet_id == NULL)
            {
                // 点在了桌面或快捷栏上
                // do nothing
            }
            else
            {
                // 点在了窗口上
                ms_dec.sht_now = get_sheet(sht_img, ms_dec.sheet_id);
                lift_sheet(sht_img, ms_dec.sheet_id);
                refresh_part_sheet(sht_img, ms_dec.sht_now->bx, ms_dec.sht_now->by,
                                   ms_dec.sht_now->bx + ms_dec.sht_now->width,
                                   ms_dec.sht_now->by + ms_dec.sht_now->height);
            }
        }
        else if (ms_dec.win_move_lock)
        {
            int his_x = ms_dec.sht_now->bx;
            int his_y = ms_dec.sht_now->by;
            // 移动窗口
            ms_dec.sht_now->bx = mouse_x - ms_dec.win_x_offset;
            ms_dec.sht_now->by = mouse_y - ms_dec.win_y_offset;
            refresh_part_sheet(sht_img, ms_dec.sht_now->bx, ms_dec.sht_now->by,
                               ms_dec.sht_now->bx + ms_dec.sht_now->width, ms_dec.sht_now->by + ms_dec.sht_now->height);
            refresh_part_sheet(sht_img, his_x, his_y, his_x + ms_dec.sht_now->width, his_y + ms_dec.sht_now->height);
        }
    }

    ms_dec.need_flush = false;
}

bool mousedecode(uint8_t data)
{
    if (ms_dec.phase == 0)
    {
        if (data == 0xfa) { ms_dec.phase = 1; }
        return false;
    }
    if (ms_dec.phase == 1)
    {
        if ((data & 0xc8) == 0x08)
        {
            ms_dec.buf[0] = data;
            ms_dec.phase  = 2;
        }
        return 0;
    }
    if (ms_dec.phase == 2)
    {
        ms_dec.buf[1] = data;
        ms_dec.phase  = 3;
        return 0;
    }
    if (ms_dec.phase == 3)
    {
        ms_dec.buf[2] = data;
        ms_dec.phase  = 1;
        ms_dec.btn    = ms_dec.buf[0] & 0x07;
        ms_dec.x      = ms_dec.buf[1];
        ms_dec.y      = ms_dec.buf[2];

        if ((ms_dec.buf[0] & 0x10) != 0) ms_dec.x |= 0xffffff00;
        if ((ms_dec.buf[0] & 0x20) != 0) ms_dec.y |= 0xffffff00;
        ms_dec.y = -ms_dec.y;
        return true;
    }
    return false;
}

int get_mouse_x()
{
    return mouse_x;
}

int get_mouse_y()
{
    return mouse_y;
}

void mouse_init()
{
    wait_KB_write();
    outb(PORT_KB_CMD, KB_EN_MOUSE_INTFACE);

    delay_ms(100);

    wait_KB_write();
    outb(PORT_KB_CMD, KB_SEND2MOUSE);
    wait_KB_write();
    outb(PORT_KB_DATA, MOUSE_EN);

    delay_ms(100);

    wait_KB_write();
    outb(PORT_KB_CMD, KBCMD_WRITE_CMD);
    wait_KB_write();
    outb(PORT_KB_DATA, KB_INIT_MODE);

    mouse_x      = (get_hor() - 16) / 2;
    mouse_y      = (get_ver() - 28 - 29) / 2;
    ms_dec.phase = 0;

    ms_dec.win_move_lock = false;
    ms_dec.sht_now       = NULL;
    delay_s(3);
}

void draw_mouse(SHEET_INFO *sht, uint16_t *sheet_id)
{
    for (int y = 0; y < 21; y++)
    {
        for (int x = 0; x < 10; x++)
        {
            if (mouse[y][x] == '.') { draw_point(sht, sheet_id, x, y, {0, 0, 0, 0}); }
            else if (mouse[y][x] == 'w') { draw_point(sht, sheet_id, x, y, WHITE); }
            else if (mouse[y][x] == '@') { draw_point(sht, sheet_id, x, y, BLACK); }
        }
    }
}

void move_mouse(SHEET_INFO *sht, uint16_t *sheet_id, int px, int py)
{
    setBX(sht, sheet_id, px);
    setBY(sht, sheet_id, py);
}
