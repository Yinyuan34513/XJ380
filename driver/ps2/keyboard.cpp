#include "../../include/global/kernel/keyboard/keyboard.h"

struct keyboard_buf kb_fifo;
UINT8 keyboard_code[256] = {
    0, KEY_ESC, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', KEY_BACKSPACE,
    KEY_TAB, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', KEY_ENTER,
    KEY_CTRL, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    KEY_SHIFT, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', KEY_SHIFT,
    '*', KEY_ALT, KEY_SPACE, KEY_CAPS, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10,
    KEY_NUML, KEY_SCROLL, '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.', 0, 0, 0, KEY_F11, KEY_F12}; // +0x80 = 释放状态
UINT8 keyboard_code1[256] = {                                                                                          // 按下Shift
    0, KEY_ESC, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', KEY_BACKSPACE,
    KEY_TAB, 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', KEY_ENTER,
    KEY_CTRL, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '\"', '~',
    KEY_SHIFT, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', KEY_SHIFT,
    '*', KEY_ALT, KEY_SPACE, KEY_CAPS, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10,
    KEY_NUML, KEY_SCROLL, '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.', 0, 0, 0, KEY_F11, KEY_F12};
extern "C" void c_keyboard_handler(void *regs_ptr, UINT64 error_code)
{
    UINT8 x;
    x = inb(0x60);
    if (x == 0x2a || x == 0x36)
    { // Shift按下
        kb_fifo.shift = 1;
    }
    else if (x == 0x1d)
    { // Ctrl按下
        kb_fifo.ctrl = 1;
    }
    else if (x == 0x3a)
    { // Caps Lock按下
        kb_fifo.caps = kb_fifo.caps ^ 1;
    }
    else if (x == 0xaa || x == 0xb6)
    { // Shift松开
        kb_fifo.shift = 0;
    }
    else if (x == 0x9d)
    { // Ctrl按下
        kb_fifo.ctrl = 0;
    }
    if (x < 0x80)
    {
        write_serial_string("scan code: ");
        write_serial_hex(x);
        write_serial_string("\n");
    }

    if (kb_fifo.p_head == kb_fifo.buf + KB_BUF_SIZE)
    {
        kb_fifo.p_head = kb_fifo.buf;
    }
    *kb_fifo.p_head = x;
    kb_fifo.count++;
    kb_fifo.p_head++;
    send_eoi();
}

UINT8 get_keyboard_input()
{
    if (kb_fifo.p_tail != kb_fifo.p_head)
    {
        // 有没有读的
        UINT8 temp = keyboard_code[*kb_fifo.p_tail];
        if (kb_fifo.shift == 1 || kb_fifo.caps == 1)
        {
            temp = keyboard_code1[*kb_fifo.p_tail];
        }

        if (kb_fifo.p_tail == kb_fifo.buf + KB_BUF_SIZE)
        {
            kb_fifo.p_tail = kb_fifo.buf;
        }
        kb_fifo.p_tail++;
        return temp;
    }
    return NULL;
}

void keyboard_init()
{
    kb_fifo.p_head = kb_fifo.buf;
    kb_fifo.p_tail = kb_fifo.buf;
    kb_fifo.count = 0;
    xmemset(kb_fifo.buf, 0, KB_BUF_SIZE);
    wait_KB_write();
    outb(PORT_KB_CMD, KBCMD_WRITE_CMD);
    wait_KB_read();
    outb(PORT_KB_DATA, KB_INIT_MODE);
}

/*
*  鼠标
*
*/
mouse_dec ms_dec;
int mouse_x;
int mouse_y;
extern "C" void c_mouse_handler(void *regs_ptr, UINT64 error_code)
{
    send_eoi();
        UINT8 data = inb(PORT_KB_DATA);

    if (mousedecode(data)) {
        mouse_x += ms_dec.x;
        mouse_y += ms_dec.y;
        if (mouse_x < 0) mouse_x = 0;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_x > get_hor() - 1) mouse_x = get_hor() - 1;
        if (mouse_y > get_ver() - 1) mouse_y = get_ver() - 1;

        ms_dec.left = false;
        ms_dec.center = false;
        ms_dec.right = false;

        if(ms_dec.btn & 0x01){
            ms_dec.left = true;
        }
        if(ms_dec.btn & 0x02){
            ms_dec.right = true;
        }
        if(ms_dec.btn & 0x04) {
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
    if (ms_dec.phase == 0) {
        if (data == 0xfa) {
            ms_dec.phase = 1;
        }
        return false;
    }
    if (ms_dec.phase == 1) {
        if ((data & 0xc8) == 0x08) {
            ms_dec.buf[0] = data;
            ms_dec.phase = 2;
        }
        return 0;
    }
    if (ms_dec.phase == 2) {
        ms_dec.buf[1] = data;
        ms_dec.phase = 3;
        return 0;
    }
    if (ms_dec.phase == 3) {
        ms_dec.buf[2] = data;
        ms_dec.phase = 1;
        ms_dec.btn = ms_dec.buf[0] & 0x07;
        ms_dec.x = ms_dec.buf[1];
        ms_dec.y = ms_dec.buf[2];

        if ((ms_dec.buf[0] & 0x10) != 0) ms_dec.x |= 0xffffff00;
        if ((ms_dec.buf[0] & 0x20) != 0) ms_dec.y |= 0xffffff00;
        ms_dec.y = -ms_dec.y;
        return true;
    }
    return false;
}
int get_mouse_x() {
    return mouse_x;
}

int get_mouse_y() {
    return mouse_y;
}
void Mouse_init()
{
    wait_KB_write();
    outb(PORT_KB_CMD, KB_EN_MOUSE_INTFACE);
    for (int i = 0; i < 1000; i++)
        for (int j = 0; j < 1000; j++)
            __asm__ volatile("nop");

    wait_KB_write();
    outb(PORT_KB_CMD, KB_SEND2MOUSE);
    wait_KB_write();
    outb(PORT_KB_DATA, MOUSE_EN);

    for (int i = 0; i < 1000; i++)
        for (int j = 0; j < 1000; j++)
            __asm__ volatile("nop");

    wait_KB_write();
    outb(PORT_KB_CMD, KBCMD_WRITE_CMD);
    wait_KB_write();
    outb(PORT_KB_DATA, KB_INIT_MODE);
    mouse_x = (get_hor() - 16) / 2;
    mouse_y = (get_ver() - 28 - 29) / 2;
    ms_dec.phase = 0;
}