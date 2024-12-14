#include "../../include/include.h"
void init_8259A()
{
    outb(INT_M_CTL, 0x11);
    outb(INT_S_CTL, 0x11);

    outb(INT_M_CTLMASK, INT_VECTOR_IRQ0);
    outb(INT_S_CTLMASK, INT_VECTOR_IRQ8);

    outb(INT_M_CTLMASK, 0x4);
    outb(INT_S_CTLMASK, 0x2);

    outb(INT_M_CTLMASK, 0x1);
    outb(INT_S_CTLMASK, 0x1);

    outb(INT_M_CTLMASK, 0xFF);
    outb(INT_S_CTLMASK, 0xFF);
}

void disable_8259A()
{
    io_mfence();
    outb(INT_M_CTLMASK, 0xff);
    io_mfence();
    outb(INT_S_CTLMASK, 0xff);
    io_mfence();
}
