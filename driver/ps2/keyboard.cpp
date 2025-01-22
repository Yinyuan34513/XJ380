#include "../../include/global/kernel/keyboard/keyboard.h"

struct keyboard_buf kb_fifo;

char keyboard_code[256] = {
	0, (char)KEY_ESC, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', (char)KEY_BACKSPACE,
	(char)KEY_TAB, 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', (char)KEY_ENTER,
	(char)KEY_CTRL, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 
	(char)KEY_SHIFT, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', (char)KEY_SHIFT, 
	'*', (char)KEY_ALT, (char)KEY_SPACE, (char)KEY_CAPS, (char)KEY_F1, (char)KEY_F2, (char)KEY_F3, (char)KEY_F4, 
	(char)KEY_F5, (char)KEY_F6, (char)KEY_F7, (char)KEY_F8, (char)KEY_F9, (char)KEY_F10,
	(char)KEY_NUML, (char)KEY_SCROLL, '7', '8', '9', '-', '4', '5', '6', '+', '1', '2', '3', '0', '.', 0, 0, 0, 
	(char)KEY_F11, (char)KEY_F12
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

char get_keyboard_input() {
    if (kb_fifo.p_tail != kb_fifo.p_head) {
        // 有没有读的
        char temp = keyboard_code[*kb_fifo.p_tail];
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