#include <cpu/longm.h>
#include <proto.hpp>
#include <stdint.h>
#include <task/pcb.h>

int lookup_kallsyms(uint64_t address)
{
    write_serial_string("backtrace address:");
    write_serial_hex(address);
    write_serial_string("\n");
    return 0;
}

void backtrace(struct X64_REGS *regs)
{
    if (regs == NULL) return;
    if (regs->rbp < 0xFFFF800000000000)
    {
        write_serial_string("Fault in user land\n");
        return;
    }

    write_serial_string("Kernel backtrace:\n");

    lookup_kallsyms(regs->rip);
    uint64_t rbp = regs->rbp;
    tcb_t task = get_current_task();
    for (int i = 0; i < 8; i++)
    {
        if ((rbp & (sizeof(uint64_t) - 1)) != 0) break;
        if (rbp < regs->rsp) break;

        bool in_known_stack = task == NULL;
        if (task != NULL)
        {
            uint64_t kernel_stack_base = task->kernel_stack - KERNEL_STACK_SIZE;
            uint64_t syscall_stack_base = task->syscall_stack - KERNEL_STACK_SIZE;
            in_known_stack = (rbp >= kernel_stack_base && rbp + sizeof(uint64_t) < task->kernel_stack) ||
                             (rbp >= syscall_stack_base && rbp + sizeof(uint64_t) < task->syscall_stack);
        }
        if (!in_known_stack) break;

        uint64_t *frame = (uint64_t *)rbp;
        uint64_t next = frame[0];
        uint64_t ret_address = frame[1];
        if (lookup_kallsyms(ret_address))
            break;

        if (next <= rbp || next - rbp > KERNEL_STACK_SIZE) break;
        rbp = next;
    }

    write_serial_string("Kernel backtrace end\n");
}
