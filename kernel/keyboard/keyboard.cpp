#include "../../include/global/kernel/keyboard/keyboard.h"
static struct keyboard_buf * p_kb =NULL;
extern "C" void c_keyboard_handler(void *regs_ptr,UINT64 error_code) {
    UINT8 x;
    x=inb(0x60);
    write_serial_string("key: ");
    write_serial_hex(x);
    write_serial_string("\n");
    if(p_kb->p_head==p_kb->buf+KB_BUF_SIZE)
    {
        p_kb->p_head=p_kb->buf;
    }
    *p_kb->p_head=x;
    p_kb->count++;
    p_kb->p_head++;
    send_eoi();
}
void keyboard_init()
{
    p_kb = (struct keyboard_buf*)malloc(sizeof(struct keyboard_buf));
    p_kb->p_head = p_kb->buf;
    p_kb->p_tail = p_kb->buf;
    p_kb->count = 0;
    xmemset(p_kb->buf,0,KB_BUF_SIZE);
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