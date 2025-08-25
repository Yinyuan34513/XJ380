#include <proto.hpp>
extern mouse_dec ms_dec;

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

extern "C" registers_t* c_keyboard_handler(registers_t *reg)
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

    WINDOWLS *Window_now = sht_found_win(xwmii, sht_img, ms_dec.sht_now);
    MsgPrcor WinMPf = Window_now->WinMPf;

    if ((UINT64)(WinMPf) != NULL && Window_now != NULL) {
        UINT8 temp = keyboard_code[*kb_fifo.p_tail];
        if (kb_fifo.shift == 1 || kb_fifo.caps == 1)
        {
            temp = keyboard_code1[*kb_fifo.p_tail];
        }
        write_serial_string("WMPF Address: ");
        write_serial_hex((UINT64)(WinMPf));
        write_serial_string("\n");
        // WinMPf(MSG_CHAR, 0, temp);
    }

    kb_fifo.count++;
    kb_fifo.p_head++;
    send_eoi();
    return reg;
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
    memset(kb_fifo.buf, 0, KB_BUF_SIZE);
    wait_KB_write();
    outb(PORT_KB_CMD, KBCMD_WRITE_CMD);
    wait_KB_read();
    outb(PORT_KB_DATA, KB_INIT_MODE);
}
