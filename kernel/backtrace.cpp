#include "../include/backtrace.h"

extern unsigned long kallsyms_addresses[] __attribute__((weak));
extern long kallsyms_syms_num __attribute__((weak));
extern long kallsyms_index[] __attribute__((weak));
extern char *kallsyms_names __attribute__((weak));

int lookup_kallsyms(unsigned long address)
{
    int index = 0;
    char *string = (char *)&kallsyms_names;
    for (index = 0; index < kallsyms_syms_num; index++)
        if (address > kallsyms_addresses[index] && address <= kallsyms_addresses[index + 1] && address >= 0x100000)
            break;
    if (index < kallsyms_syms_num)
    {
        write_serial_string("backtrace address:");
        write_serial_hex(address);
        write_serial_string(" (+) ");
        write_serial_dec(address - kallsyms_addresses[index]);
        write_serial_string("\tbacktrace function:");
        write_serial_string(&string[kallsyms_index[index]]);
        write_serial_string("(");
        write_serial_hex(kallsyms_addresses[index]);
        write_serial_string(")\n");
        return 0;
    }
    else
        return -1;
}

void backtrace(struct X64_REGS *regs)
{
    unsigned long *rbp = (unsigned long *)regs->rbp;
    unsigned long ret_address = *(rbp + 1);
    int i = 0;

    lookup_kallsyms(regs->rip);
    for (i = 0; i < 32; i++)
    {
        if (lookup_kallsyms(ret_address))
            break;
        rbp = (unsigned long *)*rbp;
        ret_address = *(rbp + 1);
    }
}