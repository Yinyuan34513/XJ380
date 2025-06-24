#include "../include/backtrace.h"

extern uint64_t kallsyms_addresses[] __attribute__((weak));
extern long     kallsyms_syms_num __attribute__((weak));
extern long     kallsyms_index[] __attribute__((weak));
extern char    *kallsyms_names __attribute__((weak));

extern uint8_t _kernel_start[];

int lookup_kallsyms(uint64_t address, int i) {
    int   index  = 0;
    char *string = (char *)&kallsyms_names;
    for (index = 0; index < kallsyms_syms_num; index++)
        if (address > kallsyms_addresses[index] && address <= kallsyms_addresses[index + 1] &&
            address >= (uint64_t)&_kernel_start)
            break;
    if (index < kallsyms_syms_num) {
        write_serial_string("backtrace address:");
        write_serial_hex(address);
        write_serial_string(" (+) ");
        write_serial_dec(address - kallsyms_addresses[index]);
        write_serial_string("\tbacktrace function:");
        write_serial_string(&string[kallsyms_index[index]]);
        write_serial_string("(");
        write_serial_hex(kallsyms_addresses[index]);
        write_serial_string(")\n");

        WriteString(*fbc_addr, 8, 400 + i * 60, "backtrace address:", {0, 0, 0});
        WriteHex(*fbc_addr, 200, 400 + i * 60, address, {0, 0, 0});
        WriteString(*fbc_addr, 8, 420 + i * 60, "backtrace function:", {0, 0, 0});
        WriteString(*fbc_addr, 200, 420 + i * 60, &string[kallsyms_index[index]], {0, 0, 0});
        WriteString(*fbc_addr, 8, 440 + i * 60, "--------------------------------", {0, 0, 0});
        return 0;
    } else
        return -1;
}

void backtrace(struct X64_REGS *regs) {
    uint64_t *rbp = (uint64_t *)regs->rbp;
    if (regs->rbp < 0xFFFF800000000000) {
        write_serial_string("Fault in user land\n");
        return;
    }
    uint64_t ret_address = *(rbp + 1);
    int      i           = 0;

    write_serial_string("Kernel backtrace:\n");

    lookup_kallsyms(regs->rip, 0);
    for (i = 0; i < 8; i++) {
        if (lookup_kallsyms(ret_address, i + 1)) break;

        if ((uint64_t)rbp >= current_task->thread->rsp0 || (uint64_t)rbp < regs->rsp) break;

        if (rbp == nullptr) { break; }
        rbp = (uint64_t *)*rbp;
        if (rbp == nullptr) { break; }
        ret_address = *(rbp + 1);
    }

    write_serial_string("Kernel backtrace end\n");
}