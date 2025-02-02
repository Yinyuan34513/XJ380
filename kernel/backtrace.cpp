#include "../include/backtrace.h"

extern uint64_t kallsyms_addresses[] __attribute__((weak));
extern long kallsyms_syms_num __attribute__((weak));
extern long kallsyms_index[] __attribute__((weak));
extern char *kallsyms_names __attribute__((weak));

extern uint8_t _kernel_start[];

int lookup_kallsyms(uint64_t address)
{
    int index = 0;
    char *string = (char *)&kallsyms_names;
    for (index = 0; index < kallsyms_syms_num; index++)
        if (address > kallsyms_addresses[index] && address <= kallsyms_addresses[index + 1] && address >= (uint64_t)&_kernel_start)
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
    uint64_t *rbp = (uint64_t *)regs->rbp;
    uint64_t ret_address = *(rbp + 1);
    int i = 0;

    write_serial_string("Kernel backtrace:\n");

    lookup_kallsyms(regs->rip);
    for (i = 0; i < 8; i++)
    {
        if (lookup_kallsyms(ret_address))
            break;

        if ((uint64_t)rbp >= current_task->thread->rsp0 || (uint64_t)rbp < regs->rsp)
            break;

        if (rbp == nullptr)
        {
            break;
        }
        rbp = (uint64_t *)*rbp;
        if (rbp == nullptr)
        {
            break;
        }
        ret_address = *(rbp + 1);
    }

    write_serial_string("Kernel backtrace end\n");
}