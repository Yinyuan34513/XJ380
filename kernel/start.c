#include "include.h"

PUBLIC void cstart()
{
    init_gdt();
    init_idt();
}