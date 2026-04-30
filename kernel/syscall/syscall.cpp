#include "../build_settings.h"
#include <cpu/fsgsbase.h>
#include <cpu/msr.h>
#include <errno.h>
#include <fs/vfs/vfs.h>
#include <math.hpp>
#include <mm/lazyalloc.h>
#include <mm/vma.h>
#include <pctable/gdt.h>
#include <pctable/idt.h>
#include <pipe.h>
#include <proto.hpp>
#include <rtc.h>
#include <syscall/signal.h>
#include <syscall/syscall.h>
#include <task/poll.h>
#include <ttf.h>

// MSR寄存器地址定义
#define MSR_EFER         0xC0000080 // EFER MSR寄存器
#define MSR_STAR         0xC0000081 // STAR MSR寄存器
#define MSR_LSTAR        0xC0000082 // LSTAR MSR寄存器
#define MSR_SYSCALL_MASK 0xC0000084 // SYSCALL_MASK MSR寄存器
#define EFER_NXE         (1 << 11)
void init_syscall()
{
    uint64_t efer;

    // 1. 启用 EFER.SCE (System Call Extensions)
    efer  = rdmsr(MSR_EFER);
    efer |= 1; // 设置 SCE 位
    wrmsr(MSR_EFER, efer);

    uint16_t cs_sysret_cmp  = SELECTOR_USER_CS - 16;
    uint16_t ss_sysret_cmp  = SELECTOR_USER_DS - 8;
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
    star          = ((uint64_t)(SELECTOR_USER_DS - 8) << 48) | // SYSRET 的基础 CS
                    ((uint64_t)SELECTOR_KERNEL_CS << 32);      // SYSCALL 的 CS
    wrmsr(MSR_STAR, star);

    // 3. 设置 LSTAR MSR (系统调用入口点)
    wrmsr(MSR_LSTAR, (uint64_t)syscall_handler);

    // 4. 设置 SYSCALL_MASK MSR (RFLAGS 掩码)
    wrmsr(MSR_SYSCALL_MASK, (1 << 9));

    efer |= EFER_NXE;
    wrmsr(MSR_EFER, efer);
}

// regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8, regs->r9
extern "C" uint64_t c_syscall_handler(struct X64_REGS *regs, uint64_t user_rsp)
{
    regs->rip    = regs->rcx;
    regs->cs     = SELECTOR_USER_CS;
    regs->rflags = regs->r11;
    regs->rsp    = user_rsp;
    regs->ss     = SELECTOR_USER_DS;

    uint64_t syscall_number = regs->rax;

    switch (syscall_number)
    {
    /* XJ380API POSIX Edition */
    // case SYS_READ:
    //     regs->rax = (uint64_t)sys_read(regs->rdi, (uint8_t *)regs->rsi, regs->rdx);
    //     break;
    case SYS_WRITE: regs->rax = (uint64_t)sys_write(regs->rdi, (uint8_t *)regs->rsi, regs->rdx, 0, 0, 0, regs); break;
    // case SYS_OPEN:
    //     regs->rax = (uint64_t)sys_open((const char *)regs->rdi, regs->rsi, regs->rdx);
    //     break;
    // case SYS_CLOSE:
    //     regs->rax = (uint64_t)sys_close(regs->rdi);
    //     break;
    // case SYS_FSTAT:
    //     regs->rax = (uint64_t)sys_fstat((uint32_t)regs->rdi, (struct stat *)regs->rsi);
    //     break;
    // case SYS_IOCTL:
    //     regs->rax = 0;
    //     break;
    // case SYS_READV:
    // {
    //     int fd = regs->rdi;
    //     struct iovec *iovec = (struct iovec *)regs->rsi;
    //     size_t count = regs->rdx;
    //     regs->rax = sys_readv(fd, iovec, count);
    //     break;
    // }
    // case SYS_WRITEV:
    // {
    //     int fd = regs->rdi;
    //     struct iovec *iovec = (struct iovec *)regs->rsi;
    //     size_t count = regs->rdx;
    //     regs->rax = sys_writev(fd, iovec, count);
    //     break;
    // }
    case SYS_READ: regs->rax = (uint64_t)sys_read(regs->rdi, (uint8_t *)regs->rsi, regs->rdx, 0, 0, 0, regs); break;
    case SYS_OPEN: regs->rax = (uint64_t)sys_open((char *)regs->rdi, regs->rsi, regs->rdx, 0, 0, 0, regs); break;
    case SYS_CLOSE: regs->rax = (uint64_t)sys_close(regs->rdi, 0, 0, 0, 0, 0, regs); break;
    case SYS_POLL:
        regs->rax = (uint64_t)sys_poll((struct pollfd *)regs->rdi, regs->rsi, regs->rdx, 0, 0, 0, regs);
        break;
    case SYS_SOCKET: regs->rax = (uint64_t)sys_socket(regs->rdi, regs->rsi, regs->rdx, 0, 0, 0, regs); break;
    case SYS_CONNECT:
        regs->rax = (uint64_t)sys_connect(regs->rdi, (const struct sockaddr *)regs->rsi, regs->rdx, 0, 0, 0, regs);
        break;
    case SYS_ACCEPT:
        regs->rax =
            (uint64_t)sys_accept(regs->rdi, (struct sockaddr *)regs->rsi, (socklen_t *)regs->rdx, 0, 0, 0, regs);
        break;
    case SYS_STAT: regs->rax = (uint64_t)sys_stat((char *)regs->rdi, (struct stat *)regs->rsi, 0, 0, 0, 0, regs); break;
    case SYS_LSTAT:
        regs->rax = (uint64_t)sys_stat((char *)regs->rdi, (struct stat *)regs->rsi, 0, 0, 0, 0, regs);
        break;
    case SYS_FSTAT: regs->rax = (uint64_t)sys_fstat(regs->rdi, (struct stat *)regs->rsi, 0, 0, 0, 0, regs); break;
    case SYS_IOCTL: regs->rax = (uint64_t)sys_ioctl(regs->rdi, regs->rsi, (void *)regs->rdx, 0, 0, 0, regs); break;
    case SYS_READV:
        regs->rax = (uint64_t)sys_readv(regs->rdi, (struct iovec *)regs->rsi, regs->rdx, 0, 0, 0, regs);
        break;
    case SYS_WRITEV:
        regs->rax = (uint64_t)sys_writev(regs->rdi, (struct iovec *)regs->rsi, regs->rdx, 0, 0, 0, regs);
        break;
    case SYS_CREAT: regs->rax = (uint64_t)sys_open((char *)regs->rdi, regs->rsi, O_CREAT, 0, 0, 0, regs); break;
    case SYS_FCNTL: regs->rax = (uint64_t)sys_fcntl(regs->rdi, regs->rsi, regs->rdx, 0, 0, 0, regs); break;
    case SYS_MKDIR: regs->rax = (uint64_t)sys_mkdir((char *)regs->rdi, regs->rsi, 0, 0, 0, 0, regs); break;
    case SYS_RMDIR: regs->rax = (uint64_t)sys_rmdir(regs->rdi, 0, 0, 0, 0, 0, regs); break;
    case SYS_UNLINK: regs->rax = (uint64_t)sys_unlink(regs->rdi, 0, 0, 0, 0, 0, regs); break;
    case SYS_CHDIR: regs->rax = (uint64_t)sys_chdir((char *)regs->rdi, 0, 0, 0, 0, 0, regs); break;
    case SYS_GETCWD: regs->rax = (uint64_t)sys_getcwd((char *)regs->rdi, regs->rsi, 0, 0, 0, 0, regs); break;
    case SYS_LSEEK: regs->rax = (uint64_t)sys_lseek(regs->rdi, regs->rsi, regs->rdx, 0, 0, 0, regs); break;
    case SYS_GETDENTS:
        regs->rax = (uint64_t)sys_getdents(regs->rdi, (dirent *)regs->rsi, regs->rdx, 0, 0, 0, regs);
        break;
    case SYS_PIPE: regs->rax = (uint64_t)sys_pipe((int *)regs->rdi, 0, 0, 0, 0, 0, regs); break;
    case SYS_PIPE2: regs->rax = (uint64_t)sys_pipe2((int *)regs->rdi, regs->rsi, 0, 0, 0, 0, regs); break;
    case SYS_SELECT:
        regs->rax = (uint64_t)sys_select(regs->rdi, (uint8_t *)regs->rsi, (uint8_t *)regs->rdx, (uint8_t *)regs->r10,
                                         (struct timeval *)regs->r8, 0, regs);
        break;
    case SYS_PSELECT6:
        regs->rax = (uint64_t)sys_pselect6(regs->rdi, (fd_set *)regs->rsi, (fd_set *)regs->rdx, (fd_set *)regs->r10,
                                           (struct timespec *)regs->r8, (WeirdPselect6 *)regs->r9, regs);
        break;
    case SYS_DUP: regs->rax = (uint64_t)sys_dup(regs->rdi, 0, 0, 0, 0, 0, regs); break;
    case SYS_DUP2: regs->rax = (uint64_t)sys_dup2(regs->rdi, regs->rsi, 0, 0, 0, 0, regs); break;
    case SYS_SHUTDOWN: regs->rax = (uint64_t)sys_shutdown(regs->rdi, regs->rsi, 0, 0, 0, 0, regs); break;
    case SYS_BIND:
        regs->rax = (uint64_t)sys_bind(regs->rdi, (const struct sockaddr *)regs->rsi, regs->rdx, 0, 0, 0, regs);
        break;
    case SYS_LISTEN: regs->rax = (uint64_t)sys_listen(regs->rdi, regs->rsi, 0, 0, 0, 0, regs); break;

    case SYS_RT_SIGACTION:
        regs->rax = (uint64_t)sys_rt_sigaction(regs->rdi, (sigaction_t *)regs->rsi, (sigaction_t *)regs->rdx, regs->r10,
                                               0, 0, regs);
        break;
    case SYS_RT_SIGPROCMASK:
        regs->rax =
            (uint64_t)sys_rt_sigprocmask(regs->rdi, (sigset_t *)regs->rsi, (sigset_t *)regs->rdx, 0, 0, 0, regs);
        break;
    case SYS_RT_SIGRETURN: regs->rax = (uint64_t)sys_sigret(0, 0, 0, 0, 0, 0, regs); break;

    case SYS_FORK: regs->rax = (uint64_t)process_fork(regs, false, 0); break;
    case SYS_VFORK: regs->rax = (uint64_t)process_fork(regs, true, 0); break;
    case SYS_EXECVE:
        regs->rax = (uint64_t)process_execve((char *)regs->rdi, (char **)regs->rsi, (char **)regs->rdx);
        break;
    case SYS_EXIT: sys_exit(regs->rdi, 0, 0, 0, 0, 0, regs); break;
    case SYS_EXIT_GROUP: sys_exit_group(regs->rdi, 0, 0, 0, 0, 0, regs); break;
    case SYS_TGKILL: regs->rax = (uint64_t)sys_tgkill(regs->rdi, regs->rsi, regs->rdx, 0, 0, 0, regs); break;
    case SYS_GETPID: regs->rax = get_current_task()->parent_group->pid; break;
    case SYS_GETTID: regs->rax = get_current_task()->tid; break;
    case SYS_CLOCK_GETTIME:
        regs->rax = (uint64_t)sys_clock_gettime(regs->rdi, (struct timespec *)regs->rsi, 0, 0, 0, 0, regs);
        break;
    case SYS_WAIT4:
        regs->rax = (uint64_t)sys_wait4(regs->rdi, (int *)regs->rsi, regs->rdx, (void *)regs->r10, 0, 0, regs);
        break;
    case SYS_KILL: regs->rax = (uint64_t)sys_kill(regs->rdi, regs->rsi, 0, 0, 0, 0, regs); break;
    case SYS_UNAME: regs->rax = (uint64_t)sys_uname((struct utsname *)regs->rdi, 0, 0, 0, 0, 0, regs); break;
    case SYS_BRK: regs->rax = (uint64_t)sys_brk(regs->rdi); break;
    case SYS_MMAP:
        regs->rax = (uint64_t)sys_mmap(regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8, regs->r9, regs);
        break;
    case SYS_MPROTECT: regs->rax = (uint64_t)sys_mprotect(regs->rdi, regs->rsi, regs->rdx, 0, 0, 0, regs); break;
    case SYS_MUNMAP: regs->rax = (uint64_t)sys_munmap(regs->rdi, regs->rsi, 0, 0, 0, 0, regs); break;
    case SYS_MREMAP:
        regs->rax = (uint64_t)sys_mremap(regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8, 0, regs);
        break;
    case SYS_NANOSLEEP: regs->rax = (uint64_t)sys_nano_sleep((void *)regs->rdi, 0, 0, 0, 0, 0, regs); break;
    case SYS_CLOCK_GETRES:
        regs->rax = (uint64_t)sys_clock_getres(regs->rdi, (uint64_t)regs->rsi, 0, 0, 0, 0, regs);
        break;
    case SYS_GETPPID: regs->rax = (uint64_t)sys_getppid(0, 0, 0, 0, 0, 0, regs); break;
    case SYS_GETUID: regs->rax = (uint64_t)sys_getuid(0, 0, 0, 0, 0, 0, regs); break;
    case SYS_GETGID: regs->rax = (uint64_t)sys_getgid(0, 0, 0, 0, 0, 0, regs); break;
    case SYS_GETEUID: regs->rax = (uint64_t)sys_geteuid(0, 0, 0, 0, 0, 0, regs); break;
    case SYS_GETEGID: regs->rax = (uint64_t)sys_getegid(0, 0, 0, 0, 0, 0, regs); break;
    case SYS_GETGROUPS: regs->rax = (uint64_t)sys_getgroups(regs->rdi, (int *)regs->rsi, 0, 0, 0, 0, regs); break;
    case SYS_GETPGID: regs->rax = (uint64_t)sys_getpgid(regs->rdi, 0, 0, 0, 0, 0, regs); break;
    case SYS_SETPGID: regs->rax = (uint64_t)sys_setpgid(regs->rdi, regs->rsi, 0, 0, 0, 0, regs); break;
    case SYS_SENDFILE:
        regs->rax = (uint64_t)sys_sendfile(regs->rdi, regs->rsi, (uint64_t *)regs->rdx, regs->r10, 0, 0, regs);
        break;
    case SYS_STATFS:
        regs->rax = (uint64_t)sys_statfs((char *)regs->rdi, (struct statfs *)regs->rsi, 0, 0, 0, 0, regs);
        break;
    case SYS_MINCORE: regs->rax = (uint64_t)sys_mincore(regs->rdi, regs->rsi, regs->rdx, 0, 0, 0, regs); break;
    case SYS_OPENAT: regs->rax = (uint64_t)sys_openat(regs->rdi, regs->rsi, regs->rdx, regs->r10, 0, 0, regs); break;
    case SYS_NEWFSTATAT:
        regs->rax =
            (uint64_t)sys_newfstatat(regs->rdi, (char *)regs->rsi, (struct stat *)regs->rdx, regs->r10, 0, 0, regs);
        break;
    case SYS_STATX:
        regs->rax =
            (uint64_t)sys_statx(regs->rdi, (char *)regs->rsi, regs->rdx, regs->r10, (struct statx *)regs->r8, 0, regs);
        break;
    case SYS_ACCESS: regs->rax = (uint64_t)sys_access((char *)regs->rdi, 0, 0, 0, 0, 0, regs); break;
    case SYS_FACCESSAT: regs->rax = (uint64_t)sys_faccessat(regs->rdi, regs->rsi, regs->rdx, 0, 0, 0, regs); break;
    case SYS_FACCESSAT2:
        regs->rax = (uint64_t)sys_faccessat2(regs->rdi, regs->rsi, regs->rdx, regs->r10, 0, 0, regs);
        break;
    case SYS_UNLINKAT: regs->rax = (uint64_t)sys_unlinkat(regs->rdi, regs->rsi, regs->rdx, 0, 0, 0, regs); break;
    case SYS_READLINK: regs->rax = (uint64_t)sys_readlink(regs->rdi, regs->rsi, regs->rdx, 0, 0, 0, regs); break;
    case SYS_LINK: regs->rax = (uint64_t)sys_link((char *)regs->rdi, (char *)regs->rsi, 0, 0, 0, 0, regs); break;
    case SYS_SYMLINK: regs->rax = (uint64_t)sys_symlink((char *)regs->rdi, (char *)regs->rsi, 0, 0, 0, 0, regs); break;
    case SYS_RENAME: regs->rax = (uint64_t)sys_rename((char *)regs->rdi, (char *)regs->rsi, 0, 0, 0, 0, regs); break;

    case SYS_ARCH_PRCTL:
        regs->rax = (uint64_t)sys_arch_prctl(regs->rdi, regs->rsi, 0, 0, 0, 0, regs);
        break;
        // case SYS_SET_TID_ADDRESS:
        //     // todo
        //     regs->rax = get_current_task()->parent_group->pid;
        //     break;
        // case SYS_SET_ROBUST_LIST:
        //     // todo
        //     regs->rax = 0;
        //     break;
        // case SYS_RSEQ:
        //     // todo
        //     regs->rax = 0;
        //     break;

    /* XJ380API XAPI Edition */
    case XAPI_OUTPUT: do_xapi_Output((char *)regs->rdi); break;
    case XAPI_INPUT: do_xapi_Input((char *)regs->rdi); break;
    case XAPI_GETCH: regs->rax = (uint64_t)do_xapi_Getch(); break;
    case XAPI_ENDLINE: do_xapi_Endline(); break;
    case XAPI_PRINTLINE: do_xapi_Printline((char *)regs->rdi); break;
    case XAPI_PRINTF: do_xapi_Printf((char *)regs->rdi); break;
    case XAPI_OUTPUT_SERIAL: do_xapi_OutputSerial((char *)regs->rdi); break;
    case XAPI_OPEN_FILE: regs->rax = do_xapi_OpenFile(regs->rdi); break;
    case XAPI_CLOSE_FILE: do_xapi_CloseFile(regs->rdi); break;
    case XAPI_SEARCH_FILE: do_xapi_SearchFile(regs->rdi, regs->rsi, regs->rdx); break;
    case XAPI_MAKEDIR: regs->rax = (uint64_t)vfs_mkdir((char *)regs->rdi); break;
    case XAPI_CREATE_FILE: regs->rax = (uint64_t)sys_open((char *)regs->rdi, O_CREAT, 0644, 0, 0, 0, regs); break;
    case XAPI_DELETE_FILE: regs->rax = (uint64_t)sys_unlink(regs->rdi, 0, 0, 0, 0, 0, regs); break;
    case XAPI_RENAME_FILE:
        regs->rax = (uint64_t)sys_rename((char *)regs->rdi, (char *)regs->rsi, 0, 0, 0, 0, regs);
        break;
    case XAPI_READ_FILE: regs->rax = (uint64_t)do_xapi_ReadFile(regs); break;
    case XAPI_WRITE_FILE: regs->rax = (uint64_t)do_xapi_WriteFile(regs); break;
    case XAPI_FORK: regs->rax = (uint64_t)process_fork(regs, false, 0); break;
    case XAPI_EXECVE:
        regs->rax = (uint64_t)process_execve((char *)regs->rdi, (char **)regs->rsi, (char **)regs->rdx);
        break;
    case XAPI_CREATE_WINDOW: regs->rax = (uint64_t)do_xapi_CreateWindow(regs->rdi, regs->rsi); break;
    case XAPI_SET_WINDOW_TITLE: regs->rax = (uint64_t)do_xapi_SetWindowTitle(regs->rdi, regs->rsi); break;
    case XAPI_CLOSE_WINDOW: regs->rax = (uint64_t)do_xapi_CloseWindow(regs->rdi); break;
    case XAPI_SET_ICON: regs->rax = (uint64_t)do_xapi_SetIcon(regs->rdi, regs->rsi); break;
    case XAPI_GET_WIN_SZIE: regs->rax = (uint64_t)do_xapi_GetWindowSize(regs->rdi, regs->rsi, regs->rdx); break;
    case XAPI_DRAW_POINT: regs->rax = (uint64_t)do_xapi_DrawPoint(regs->rdi, regs->rsi, regs->rdx, regs->r10); break;
    case XAPI_DRAW_LINE:
        regs->rax = (uint64_t)do_xapi_DrawLine(regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8, regs->r9);
        break;
    case XAPI_DRAW_RECT:
        regs->rax = (uint64_t)do_xapi_DrawRect(regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8, regs->r9);
        break;
    case XAPI_DRAW_RECT_FILL:
        regs->rax = (uint64_t)do_xapi_DrawRect_fill(regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8, regs->r9);
        break;
    // case XAPI_DRAW_CIRCLE:
    //     regs->rax = (uint64_t)do_xapi_DrawCircle(regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8);
    //     break;
    // case XAPI_DRAW_CIRCLE_FILL:
    //     regs->rax = (uint64_t)do_xapi_DrawCircle_fill(regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8);
    //     break;
    case XAPI_DRAW_TEXT:
        regs->rax = (uint64_t)do_xapi_DrawText(regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8, regs->r9);
        break;
    case XAPI_DRAW_TEXT_L:
        regs->rax = (uint64_t)do_xapi_DrawTextl(regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8, regs->r9);
        break;
    case XAPI_DRAW_TEXT_SW:
        regs->rax = (uint64_t)do_xapi_DrawSWText(regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8, regs->r9);
        break;
    case XAPI_DRAW_BMP: do_xapi_DrawBMP(regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8, regs->r9); break;
    case XAPI_DRAW_PNG: do_xapi_DrawPNG(regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8, regs->r9); break;
    case XAPI_DRAW_PICTURE: do_xapi_DrawPicture(regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8, regs->r9); break;
    case XAPI_DRAW_SVG:
        regs->rax = (uint64_t)xapi_doDrawSvg(regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8, regs->r9);
        break;
    case XAPI_DRAW_FA:
        regs->rax = (uint64_t)xapi_doDrawFA(regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8, regs->r9);
        break;
    case XAPI_SET_MSH_PROCOR: regs->rax = (uint64_t)do_xapi_SetMsgProc(regs->rdi, regs->rsi); break;
    case XAPI_READBUFFER: do_xapi_ReadBuffer(regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8, regs->r9); break;
    case XAPI_WRITEBUFFER: do_xapi_WriteBuffer(regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8, regs->r9); break;
    case XAPI_READBUFFERA: do_xapi_ReadBufferA(regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8, regs->r9); break;
    case XAPI_WRITEBUFFERA: do_xapi_WriteBufferA(regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8, regs->r9); break;
    case XAPI_REFRESH_WINDOW: regs->rax = (uint64_t)do_xapi_RefreshWindow(regs->rdi); break;
    case XAPI_BUTTON: do_xapi_Button(regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8); break;
    case XAPI_BUTTON_EMP: do_xapi_ButtonEmp(regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8); break;
    case XAPI_DELETE_BUTTON: do_xapi_DeleteButton(regs->rdi, regs->rsi); break;
    case XAPI_GET_VERSION: do_xapi_GetSystemVersion(regs->rdi); break;
    case XAPI_GET_TIME: regs->rax = (uint64_t)do_xapi_GetTime(); break;
    case XAPI_GET_CURRENT_USER: do_xapi_GetCurrentUser(regs->rdi); break;
    case XAPI_SLEEP: do_xapi_Sleep(regs->rdi); break;
    case XAPI_CALC_TEXT_WIDTH: regs->rax = calc_ttf_length((char *)regs->rdi, regs->rsi); break;
    case XAPI_BROKEN: do_xapi_Broken((char *)regs->rdi); break;
    case XAPI_GET_TIME_X: do_xapi_GetTimeX(regs->rdi); break;
    case XAPI_GET_CPU_MODEL: get_cpu_name((char *)regs->rdi); break;
    case XAPI_GET_MEMORY_SIZE: regs->rax = do_xapi_GetMemorySize(); break;
    case XAPI_SEND_APP_MSG:
        open_interrupt;
        draw_app_message_box((char *)regs->rdi, (char *)regs->rsi);
        close_interrupt;
        break;
    case XAPI_REG_RB_MENU: do_xapi_RegisterRightButtonMenu(regs->rdi, regs->rsi, regs->rdx); break;
    case XAPI_URG_RB_MENU: do_xapi_DeleteRightButtonMenu(regs->rdi); break;
    case XAPI_REFRESH_PART_WINDOW:
        do_xapi_RefreshPartWindow(regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8);
        break;
    case XAPI_RUN: do_xapi_Run((char *)regs->rdi); break;
    case XAPI_GET_PIC_SIZE: GetPictureSize((int *)regs->rdi, (int *)regs->rsi, (char *)regs->rdx); break;
    case XAPI_MAP_MEMORY: regs->rax = do_xapi_MapMemory(regs->rdi, regs->rsi, regs->rdx); break;
    case XAPI_REMOVEDIR: sys_rmdir(regs->rdi, 0, 0, 0, 0, 0, regs); break;
    case XAPI_FLUSH_TIME: do_xapi_FlushTime(); break;
    case XAPI_GET_TASK_LIST: regs->rax = do_xapi_GetTaskList(regs->rdi, regs->rsi); break;
    case XAPI_KILL_PROCESS: regs->rax = do_xapi_KillProcess(regs->rdi); break;

    case SXAH_CHECK_TERMINAL_INIT_STATUS: regs->rax = (uint64_t)check_terminal_init_status(); break;

    /* XJ380API 隐藏版（严禁泄露） */
    case SXAH_SYSCALL_RETURN:
        // get_current_task()->parent_group->msgprci = false;
        // message_end();
        break;
    case SXAH_MARK_IS_TERMINAL: mark_process_is_terminal(); break;
    case SXAH_READ_OUTPUT_BUFFER: read_terminal_app_output_buffer((char *)regs->rdi); break;
    case SXAH_WRITE_INPUT_BUFFER: write_terminal_app_output_buffer((char *)regs->rdi); break;
    case SXAH_CHECK_INPUT_BUFFER: check_input_waiting_status(); break;
    case SXAH_UNLOCK_OUTPUT_LOCK: terminal_finish_app_output(); break;
    case SXAH_MESSAGE_ASK: regs->rax = message_ask(regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8); break;

    default:
        regs->rax = (uint64_t)-ENOSYS;
        write_serial_string("Unknown syscall index: ");
        write_serial_dec(syscall_number);
        write_serial_string("\n");
        break;
    }
    signal_deliver_pending(regs);
    return (uint64_t)regs;
}
