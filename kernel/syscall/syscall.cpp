#include "../../include/include.h"

// MSR寄存器地址定义
#define MSR_EFER 0xC0000080         // EFER MSR寄存器
#define MSR_STAR 0xC0000081         // STAR MSR寄存器
#define MSR_LSTAR 0xC0000082        // LSTAR MSR寄存器
#define MSR_SYSCALL_MASK 0xC0000084 // SYSCALL_MASK MSR寄存器

void init_syscall()
{
    uint64_t efer;

    // 1. 启用 EFER.SCE (System Call Extensions)
    efer = rdmsr(MSR_EFER);
    efer |= 1; // 设置 SCE 位
    wrmsr(MSR_EFER, efer);

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
    wrmsr(MSR_STAR, star);

    // 3. 设置 LSTAR MSR (系统调用入口点)
    wrmsr(MSR_LSTAR, (uint64_t)syscall_handler);

    // 4. 设置 SYSCALL_MASK MSR (RFLAGS 掩码)
    wrmsr(MSR_SYSCALL_MASK, (1 << 9));
}

// regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8, regs->r9
extern "C" uint64_t c_syscall_handler(struct X64_REGS *regs, uint64_t user_rsp)
{
    regs->rip = regs->rcx;
    regs->cs = SELECTOR_USER_CS;
    regs->rflags = regs->r11;
    regs->rsp = user_rsp + sizeof(struct X64_REGS);
    regs->ss = SELECTOR_USER_DS;

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
    case SYS_FSTAT:
        regs->rax = (uint64_t)sys_fstat((uint32_t)regs->rdi, (struct stat *)regs->rsi);
        break;
    case SYS_IOCTL:
        regs->rax = 0;
        break;
    case SYS_READV:
    {
        int fd = regs->rdi;
        struct iovec *iovec = (struct iovec *)regs->rsi;
        size_t count = regs->rdx;
        regs->rax = sys_readv(fd, iovec, count);
        break;
    }
    case SYS_WRITEV:
    {
        int fd = regs->rdi;
        struct iovec *iovec = (struct iovec *)regs->rsi;
        size_t count = regs->rdx;
        regs->rax = sys_writev(fd, iovec, count);
        break;
    }
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
    case SYS_EXIT_GROUP:
        regs->rax = (uint64_t)sys_exit(regs->rdi);
        break;
    case SYS_GETPID:
        regs->rax = current_task->pid;
        break;
    case SYS_GETTID:
        regs->rax = current_task->pid;
        break;
    case SYS_WAIT4:
        regs->rax = (uint64_t)sys_wait4(regs->rdi, (long *)regs->rsi, regs->rdx, (void *)regs->r10);
        break;
    case SYS_BRK:
        regs->rax = (uint64_t)sys_brk(regs->rdi);
        break;
    case SYS_MMAP:
        regs->rax = (uint64_t)sys_mmap(regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8, regs->r9);
        break;

    case SYS_ARCH_PRCTL:
        regs->rax = (uint64_t)sys_arch_prctl(regs->rdi, regs->rsi);
        break;
    case SYS_SET_TID_ADDRESS:
        // todo
        regs->rax = current_task->pid;
        break;
    case SYS_SET_ROBUST_LIST:
        // todo
        regs->rax = 0;
        break;
    case SYS_RSEQ:
        // todo
        regs->rax = 0;
        break;

    case SYS_RT_SIGPROCMASK:
        regs->rax = 0;
        break;

    case XAPI_OUTPUT:
        break;
    case XAPI_INPUTW:
        break;
    case XAPI_INPUT:
        break;
    case XAPI_ENDLINE:
        break;
    case XAPI_PRINTLINE:
        break;
    case XAPI_OPEN_FILE:
        break;
    case XAPI_CLOSE_FILE:
        break;
    case XAPI_FORK:
        regs->rax = (uint64_t)sys_fork(regs);
        break;
    case XAPI_EXECVE:
        regs->rax = (uint64_t)sys_execve(regs);
        break;
    case XAPI_CREATE_WINDOW:
        regs->rax = (uint64_t)do_xapi_CreateWindow(regs->rdi, regs->rsi);
        break;
    case XAPI_SET_WINDOW_TITLE:
        regs->rax = (uint64_t)do_xapi_SetWindowTitle(regs->rdi, regs->rsi);
        break;
    case XAPI_CLOSE_WINDOW:
        break;
    case XAPI_SET_ICON:
        break;
    case XAPI_DRAW_POINT:
        break;
    case XAPI_DRAW_LINE:
        break;
    case XAPI_DRAW_RECT:
        break;
    case XAPI_DRAW_CIRCLE:
        break;
    case XAPI_DRAW_TEXT:
        break;
    case XAPI_DRAW_BMP:
        break;
    case XAPI_DRAW_PNG:
        break;
    case XAPI_SET_MSH_PROCOR:
        break;
    case XAPI_READBUFFER:
        break;
    case XAPI_WRITEBUFFER:
        break;
    case XAPI_WRITEBUFFERA:
        break;
    case XAPI_BUTTON:
        break;
    case XAPI_BUTTON_STYLE:
        break;

    default:
        regs->rax = (uint64_t)-ENOSYS;
        write_serial_string("Unknown syscall index: ");
        write_serial_dec(syscall_number);
        write_serial_string("\n");
        break;
    }

    return user_rsp;
}
