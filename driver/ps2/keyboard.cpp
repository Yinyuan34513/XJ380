#include "../../include/global/kernel/keyboard/keyboard.h"

struct keyboard_buf kb_fifo;

UINT8 keyboard_code[256] = {
	0, KEY_ESC, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', KEY_BACKSPACE,
	KEY_TAB, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', KEY_ENTER,
	KEY_CTRL, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 
	KEY_SHIFT, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', KEY_SHIFT, 
	'*', KEY_ALT, KEY_SPACE, KEY_CAPS, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10,
	KEY_NUML, KEY_SCROLL, '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.', 0, 0, 0, KEY_F11, KEY_F12
};	// +0x80 = 释放状态

extern "C" void c_keyboard_handler(void *regs_ptr,UINT64 error_code) {
    UINT8 x;
    x=inb(0x60);
    write_serial_string("scan code: ");
    write_serial_hex(x);
    write_serial_string("\n");
    if (x < 0x80) {
        write_serial_string("key: ");
        write_serial(keyboard_code[x]);
        write_serial_string("\n");
    }
    if(kb_fifo.p_head==kb_fifo.buf+KB_BUF_SIZE)
    {
        kb_fifo.p_head=kb_fifo.buf;
    }
    *kb_fifo.p_head=x;
    kb_fifo.count++;
    kb_fifo.p_head++;
    send_eoi();
}

UINT8 get_keyboard_input() {
    if (kb_fifo.p_tail != kb_fifo.p_head) {
        // 有没有读的
        UINT8 temp = keyboard_code[*kb_fifo.p_tail];
        if (kb_fifo.p_tail == kb_fifo.buf + KB_BUF_SIZE)
        {
            kb_fifo.p_tail = kb_fifo.buf;
        }
        kb_fifo.p_tail++;

        return temp;
    }
    return NULL;
}

void keyboard_init() {
    kb_fifo.p_head = kb_fifo.buf;
    kb_fifo.p_tail = kb_fifo.buf;
    kb_fifo.count = 0;
    xmemset(kb_fifo.buf,0,KB_BUF_SIZE);
    wait_KB_write();
    outb(PORT_KB_CMD,KBCMD_WRITE_CMD);
    wait_KB_read();
    outb(PORT_KB_DATA,KB_INIT_MODE);
    for(int i=0;i<1000;i++)
    {
        for(int j=0;j<1000;j++)
        {
            __asm__ volatile("nop");
        }
    }
}