#include <cpu/msr.h>
#include <errno.h>
#include <fs/fs_syscall.h>
#include <pctable/gdt.h>
#include <pctable/idt.h>
#include <proto.hpp>
#include <syscall/syscall.h>

// MSR寄存器地址定义
#define MSR_EFER         0xC0000080 // EFER MSR寄存器
#define MSR_STAR         0xC0000081 // STAR MSR寄存器
#define MSR_LSTAR        0xC0000082 // LSTAR MSR寄存器
#define MSR_SYSCALL_MASK 0xC0000084 // SYSCALL_MASK MSR寄存器

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
           ((uint64_t)SELECTOR_KERNEL_CS << 32);               // SYSCALL 的 CS
    wrmsr(MSR_STAR, star);

    // 3. 设置 LSTAR MSR (系统调用入口点)
    wrmsr(MSR_LSTAR, (uint64_t)syscall_handler);

    // 4. 设置 SYSCALL_MASK MSR (RFLAGS 掩码)
    wrmsr(MSR_SYSCALL_MASK, (1 << 9));
}

// regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8, regs->r9
extern "C" uint64_t c_syscall_handler(struct X64_REGS *regs, uint64_t user_rsp)
{
    regs->rip    = regs->rcx;
    regs->cs     = SELECTOR_USER_CS;
    regs->rflags = regs->r11;
    regs->rsp    = user_rsp + sizeof(struct X64_REGS);
    regs->ss     = SELECTOR_USER_DS;

    uint64_t syscall_number = regs->rax;

    switch (syscall_number)
    {
    // case SYS_READ:
    //     regs->rax = (uint64_t)sys_read(regs->rdi, (uint8_t *)regs->rsi, regs->rdx);
    //     break;
    case SYS_WRITE: regs->rax = (uint64_t)sys_write(regs->rdi, (uint8_t *)regs->rsi, regs->rdx); break;
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
    case SYS_READ: regs->rax = (uint64_t)sys_read(regs->rdi, (void *)regs->rsi, regs->rdx); break;
    case SYS_OPEN: regs->rax = (uint64_t)sys_open((const char *)regs->rdi, regs->rsi, regs->rdx); break;
    case SYS_CLOSE: regs->rax = (uint64_t)sys_close(regs->rdi); break;
    case SYS_FSTAT: regs->rax = (uint64_t)sys_fstat(regs->rdi, (struct stat *)regs->rsi); break;
    case SYS_CREAT: regs->rax = (uint64_t)sys_creat((const char *)regs->rdi, regs->rsi); break;
    case SYS_MKDIR: regs->rax = (uint64_t)sys_mkdir((const char *)regs->rdi, regs->rsi); break;
    case SYS_RMDIR: regs->rax = (uint64_t)sys_rmdir((const char *)regs->rdi); break;
    case SYS_UNLINK: regs->rax = (uint64_t)sys_unlink((const char *)regs->rdi); break;
    case SYS_CHDIR: regs->rax = (uint64_t)sys_chdir((const char *)regs->rdi); break;
    case SYS_GETCWD: regs->rax = (uint64_t)sys_getcwd((char *)regs->rdi, regs->rsi); break;
    case SYS_LSEEK: regs->rax = (uint64_t)sys_lseek(regs->rdi, regs->rsi, regs->rdx); break;
    case SYS_GETDENTS: regs->rax = (uint64_t)sys_getdents(regs->rdi, regs->rsi, regs->rdx); break;
    case SYS_DUP: regs->rax = (uint64_t)sys_dup(regs->rdi); break;
    case SYS_DUP2: regs->rax = (uint64_t)sys_dup2(regs->rdi, regs->rsi); break;

    case SYS_FORK: regs->rax = (uint64_t)process_fork(regs, false, 0); break;
    case SYS_VFORK: regs->rax = (uint64_t)process_fork(regs, true, 0); break;
    case SYS_EXECVE:
        regs->rax = (uint64_t)process_execve((char *)regs->rdi, (char **)regs->rsi, (char **)regs->rdx);
        break;
    case SYS_EXIT:
        thread_exit(regs->rdi);
        break;
        // case SYS_EXIT_GROUP:
        //     regs->rax = (uint64_t)sys_exit(regs->rdi);
        //     break;
        // case SYS_GETPID:
        //     regs->rax = get_current_task()->parent_group->pid;
        //     break;
        // case SYS_GETTID:
        //     regs->rax = get_current_task()->parent_group->pid;
        //     break;
        // case SYS_WAIT4:
        //     regs->rax = (uint64_t)sys_wait4(regs->rdi, (long *)regs->rsi, regs->rdx, (void *)regs->r10);
        //     break;
        // case SYS_BRK:
        //     regs->rax = (uint64_t)sys_brk(regs->rdi);
        //     break;
        // case SYS_MMAP:
        //     regs->rax = (uint64_t)sys_mmap(regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8, regs->r9);
        //     break;

        // case SYS_ARCH_PRCTL:
        //     regs->rax = (uint64_t)sys_arch_prctl(regs->rdi, regs->rsi);
        //     break;
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

        // case SYS_RT_SIGPROCMASK:
        //     regs->rax = 0;
        //     break;

    case XAPI_OUTPUT: do_xapi_Output((char *)regs->rdi); break;
    case XAPI_INPUT: do_xapi_Input((char *)regs->rdi); break;
    case XAPI_GETCH: regs->rax = (uint64_t)do_xapi_Getch(); break;
    case XAPI_ENDLINE: do_xapi_Endline(); break;
    case XAPI_PRINTLINE: do_xapi_Printline((char *)regs->rdi); break;
    case XAPI_OPEN_FILE: break;
    case XAPI_CLOSE_FILE: break;
    case XAPI_SEARCH_FILE: do_xapi_SearchFile(regs->rdi, regs->rsi, regs->rdx); break;
    case XAPI_MAKEDIR: regs->rax = (uint64_t)vfs_mkdir((char *)regs->rdi); break;
    case XAPI_CREATE_FILE: regs->rax = (uint64_t)sys_creat((const char *)regs->rdi, 0644); break;
    case XAPI_DELETE_FILE: regs->rax = (uint64_t)sys_unlink((const char *)regs->rdi); break;
    case XAPI_RENAME_FILE: regs->rax = (uint64_t)sys_rename((const char *)regs->rdi, (const char *)regs->rsi); break;
    case XAPI_READ_FILE: {
        // 参数: filename, buffer, size, offset
        vfs_node_t node = vfs_open((const char *)regs->rdi);
        if (!node)
        {
            regs->rax = (uint64_t)-ENOENT;
            break;
        }

        if (node->type & file_dir)
        {
            vfs_close(node);
            regs->rax = (uint64_t)-EISDIR;
            break;
        }

        ssize_t ret = vfs_read(node, (void *)regs->rsi, regs->r9, regs->rdx);
        vfs_close(node);
        regs->rax = (uint64_t)ret;
        break;
    }
    case XAPI_WRITE_FILE: {
        // 参数: filename, buffer, size, offset
        vfs_node_t node = vfs_open((const char *)regs->rdi);
        if (!node)
        {
            // 文件不存在，创建它
            int create_ret = vfs_mkfile((const char *)regs->rdi);
            if (create_ret < 0)
            {
                regs->rax = (uint64_t)-ENOENT;
                break;
            }
            node = vfs_open((const char *)regs->rdi);
            if (!node)
            {
                regs->rax = (uint64_t)-ENOENT;
                break;
            }
        }

        if (node->type & file_dir)
        {
            vfs_close(node);
            regs->rax = (uint64_t)-EISDIR;
            break;
        }

        ssize_t ret = vfs_write(node, (const void *)regs->rsi, regs->r9, regs->rdx);
        vfs_close(node);
        regs->rax = (uint64_t)ret;
        break;
    }
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
    case XAPI_SET_MSH_PROCOR: regs->rax = (uint64_t)do_xapi_SetMsgProc(regs->rdi, regs->rsi); break;
    case XAPI_READBUFFER: do_xapi_ReadBuffer(regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8, regs->r9); break;
    case XAPI_WRITEBUFFER: do_xapi_WriteBuffer(regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8, regs->r9); break;
    case XAPI_READBUFFERA: do_xapi_ReadBufferA(regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8, regs->r9); break;
    case XAPI_WRITEBUFFERA: do_xapi_WriteBufferA(regs->rdi, regs->rsi, regs->rdx, regs->r10, regs->r8, regs->r9); break;
    case XAPI_REFRESH_WINDOW: regs->rax = (uint64_t)do_xapi_RefreshWindow(regs->rdi); break;
    case XAPI_BUTTON: break;
    case XAPI_BUTTON_STYLE: break;
    case XAPI_GET_VERSION: do_xapi_GetSystemVersion(regs->rdi); break;
    case XAPI_GET_TIME: regs->rax = (uint64_t)do_xapi_GetTime(); break;
    case XAPI_GET_CURRENT_USER: do_xapi_GetCurrentUser(regs->rdi); break;

    case SXAH_SYSCALL_RETURN:
        get_current_task()->parent_group->sigprci = false;
        signal_end();
        break;
    case SXAH_MARK_IS_TERMINAL: get_current_task()->parent_group->xtttp_stc.is_shell = true; break;

    default:
        regs->rax = (uint64_t)-ENOSYS;
        write_serial_string("Unknown syscall index: ");
        write_serial_dec(syscall_number);
        write_serial_string("\n");
        break;
    }

    return user_rsp;
}
