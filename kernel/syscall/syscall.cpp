#include "../../include/include.h"

// MSR寄存器地址定义
#define MSR_EFER 0xC0000080         // EFER MSR寄存器
#define MSR_STAR 0xC0000081         // STAR MSR寄存器
#define MSR_LSTAR 0xC0000082        // LSTAR MSR寄存器
#define MSR_SYSCALL_MASK 0xC0000084 // SYSCALL_MASK MSR寄存器

// 读取MSR寄存器的函数
static inline uint64_t read_msr(uint32_t msr)
{
    uint32_t low, high;
    asm volatile("rdmsr" : "=a"(low), "=d"(high) : "c"(msr));
    return ((uint64_t)high << 32) | low;
}

// 写入MSR寄存器的函数
static inline void write_msr(uint32_t msr, uint64_t value)
{
    uint32_t low = value & 0xFFFFFFFF;
    uint32_t high = value >> 32;
    asm volatile("wrmsr" : : "c"(msr), "a"(low), "d"(high));
}

void init_syscall()
{
    uint64_t efer;

    // 1. 启用 EFER.SCE (System Call Extensions)
    efer = read_msr(MSR_EFER);
    efer |= 1; // 设置 SCE 位
    write_msr(MSR_EFER, efer);

    uint16_t cs_sysret_cmp = SELECTOR_USER_CS - 16;
    uint16_t ss_sysret_cmp = SELECTOR_USER_DS - 8;
    uint16_t cs_syscall_cmp = SELECTOR_KERNEL_CS;
    uint16_t ss_syscall_cmp = SELECTOR_KERNEL_DS - 8;

    if (cs_sysret_cmp != ss_sysret_cmp)
    {
        write_serial_string("Sysret offset is not valid (1)\n");
        return;
    }

    if (cs_syscall_cmp != ss_syscall_cmp)
    {
        write_serial_string("Syscall offset is not valid (2)\n");
        return;
    }

    // 2. 设置 STAR MSR
    uint64_t star = 0;
    star = ((uint64_t)(SELECTOR_USER_DS - 8) << 48) | // SYSRET 的基础 CS
           ((uint64_t)SELECTOR_KERNEL_CS << 32);      // SYSCALL 的 CS
    write_msr(MSR_STAR, star);

    // 3. 设置 LSTAR MSR (系统调用入口点)
    write_msr(MSR_LSTAR, (uint64_t)syscall_handler);

    // 4. 设置 SYSCALL_MASK MSR (RFLAGS 掩码)
    write_msr(MSR_SYSCALL_MASK, (1 << 9));
}

extern "C" void c_syscall_handler(struct X64_REGS *regs)
{
    UINT64 syscall_number = regs->rax;

    switch (syscall_number)
    {
    case SYS_READ:
        regs->rax = (uint64_t)sys_read(regs->rdi, (uint8_t *)regs->rsi, regs->rdx);
        break;
    case SYS_WRITE:
        regs->rax = (uint64_t)sys_write(regs->rdi, (uint8_t *)regs->rsi, regs->rdx);
        break;
    case SYS_OPEN:
        regs->rax = (uint64_t)sys_open((const char *)regs->rdi, regs->rsi, regs->rdx);
        break;
    case SYS_CLOSE:
        regs->rax = (uint64_t)sys_close(regs->rdi);
        break;
    case SYS_FORK:
        regs->rax = (uint64_t)sys_fork(regs);
        break;
    case SYS_VFORK:
        regs->rax = (uint64_t)sys_vfork(regs);
        break;
    case SYS_EXECVE:
        regs->rax = (uint64_t)sys_execve(regs);
        break;
    case SYS_EXIT:
        regs->rax = (uint64_t)sys_exit(regs->rdi);
        break;
    case SYS_WAIT4:
        regs->rax = (uint64_t)sys_wait4(regs->rdi, (int *)regs->rsi, regs->rdx, (void *)regs->r10);
        break;
    default:
        regs->rax = (uint64_t)-ENOSYS;
        write_serial_string("Unknown syscall index: ");
        write_serial_dec(syscall_number);
        write_serial_string("\n");
        break;
    }
}
