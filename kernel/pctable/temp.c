#define INT_M_CTL     0x20
#define INT_M_CTLMASK 0x21
#define INT_S_CTL     0xA0
#define INT_S_CTLMASK 0xA1

#define INT_VECTOR_IRQ0         0x20
#define INT_VECTOR_IRQ8         0x28

typedef void (*irq_handler)(int irq);
irq_handler irq_table[16];

#include "../../include/include.h"

void spurious_irq(int irq)
{
    //
}

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

    int i;
    for (i = 0; i < 16; i++) irq_table[i] = spurious_irq;
}
