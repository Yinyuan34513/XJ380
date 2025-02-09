#include "../../include/include.h"
#include "../../graphics/mouse.hpp"

mouse_dec ms_dec;
int mouse_x = 0;
int mouse_y = 0;

extern "C" void c_mouse_handler(void *regs_ptr, UINT64 error_code)
{
    send_eoi();
    UINT8 data = inb(PORT_KB_DATA);

    if (mousedecode(data))
    {
        mouse_x += ms_dec.x;
        mouse_y += ms_dec.y;
        if (mouse_x < 0)
            mouse_x = 0;
        if (mouse_y < 0)
            mouse_y = 0;
        if (mouse_x > get_hor() - 1)
            mouse_x = get_hor() - 1;
        if (mouse_y > get_ver() - 1)
            mouse_y = get_ver() - 1;

        ms_dec.left = false;
        ms_dec.center = false;
        ms_dec.right = false;

        if (ms_dec.btn & 0x01)
        {
            ms_dec.left = true;
        }
        if (ms_dec.btn & 0x02)
        {
            ms_dec.right = true;
        }
        if (ms_dec.btn & 0x04)
        {
            ms_dec.center = true;
        }
    }
    // write_serial_string("mouse X:");
    // write_serial_dec(mouse_x);
    // write_serial_string("\n");
    // write_serial_string("mouse Y:");
    // write_serial_dec(mouse_y);
    // write_serial_string("\n");
}

bool mousedecode(UINT8 data)
{
    if (ms_dec.phase == 0)
    {
        if (data == 0xfa)
        {
            ms_dec.phase = 1;
        }
        return false;
    }
    if (ms_dec.phase == 1)
    {
        if ((data & 0xc8) == 0x08)
        {
            ms_dec.buf[0] = data;
            ms_dec.phase = 2;
        }
        return 0;
    }
    if (ms_dec.phase == 2)
    {
        ms_dec.buf[1] = data;
        ms_dec.phase = 3;
        return 0;
    }
    if (ms_dec.phase == 3)
    {
        ms_dec.buf[2] = data;
        ms_dec.phase = 1;
        ms_dec.btn = ms_dec.buf[0] & 0x07;
        ms_dec.x = ms_dec.buf[1];
        ms_dec.y = ms_dec.buf[2];

        if ((ms_dec.buf[0] & 0x10) != 0)
            ms_dec.x |= 0xffffff00;
        if ((ms_dec.buf[0] & 0x20) != 0)
            ms_dec.y |= 0xffffff00;
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

    mouse_x = (get_hor() - 16) / 2;
    mouse_y = (get_ver() - 28 - 29) / 2;
    ms_dec.phase = 0;
}

void draw_mouse(SHEET_INFO *sht, UINT16 *sheet_id)
{
    for (int y = 0; y < 21; y++)
    {
        for (int x = 0; x < 10; x++)
        {
            if (mouse[y][x] == '.')
            {
                draw_point(sht, sheet_id, x, y, {0, 0, 0, 0});
            }
            else if (mouse[y][x] == 'w')
            {
                draw_point(sht, sheet_id, x, y, WHITE);
            }
            else if (mouse[y][x] == '@')
            {
                draw_point(sht, sheet_id, x, y, BLACK);
            }
        }
    }
}

void move_mouse(SHEET_INFO *sht, UINT16 *sheet_id, int px, int py)
{
    setBX(sht, sheet_id, px);
    setBY(sht, sheet_id, py);
}
