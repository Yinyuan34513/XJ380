#include <ahci/ahci.h>
#include <efi/boot.h>
#include <efi/efi.h>
#include <efi/fbc.h>
#include <elf.h>
#include <errno.h>
#include <font.h>
#include <fs/fatfs/fatfs.h>
#include <fs/fs_syscall.h>
#include <fs/partition.h>
#include <fs/vfs/dev.h>
#include <fs/vfs/sys.h>
#include <fs/vfs/vfs.h>
#include <global_color.h>
#include <graphics/GOP.hpp>
#include <graphics/sheet.h>
#include <krlibc.h>
#include <mm/frame.h>
#include <pci/pci.h>
#include <proto.hpp>
#include <ps2/keyboard.h>
#include <ps2/mouse.h>
#include <rtc.h>
#include <stdint.h>
#include <syscall/syscall.h>
#include <task/pcb.h>
#include <user/user.h>
#include "../build_settings.h"

void init_process_xtttp(pcb_t pcb)
{
    // pcb->xtttp_stc->input    = (char *)malloc(1024 * sizeof(char));
    memset(pcb->xtttp_stc->output,0,sizeof(pcb->xtttp_stc->output));
    // pcb->xtttp_stc->output   = (char *)malloc(1024 * sizeof(char));
    memset(pcb->xtttp_stc->input,0,sizeof(pcb->xtttp_stc->input));
}

void init_terminal_app(struct X64_REGS *reg)
{
    uint64_t pid  = process_fork(reg, false, 0);
    tcb_t current = get_current_task();

    if (pid == 0)
    {
        // 是子进程！
        // 等待命令行初始化
        while (true)
        {
            // 这时命令行肯定是他的父进程，循环检查是否标记为命令行，检测到标记即代表可以吃了
            if (current->parent_group->parent_task->xtttp_stc->is_shell) break;
        }
        return;
    }

    char *argv[2] = {"-xj380sys-terminal-app-mode"};
    process_execve("/system/shell.elf", argv, NULL);
}

void mark_process_is_terminal()
{
    write_serial_fmt("Marking Terminal...\n");
    tcb_t current = get_current_task();
    init_process_xtttp(current->parent_group);
    current->parent_group->xtttp_stc->is_shell = true;
    current->parent_group->co = true;
}

int check_input_waiting_status()
{
    tcb_t current = get_current_task();
    return current->parent_group->parent_task->xtttp_stc->wait_for_input ? 1 : 0;
}

void read_terminal_app_output_buffer(char *str)
{
    tcb_t current = get_current_task();
    // write_serial_fmt("NAME!! %s\n",current->name);
    strcpy(str, current->parent_group->parent_task->xtttp_stc->output);
}

void write_terminal_app_output_buffer(char *str)
{
    tcb_t current = get_current_task();
    strcpy(current->parent_group->parent_task->xtttp_stc->input, str);
}

void terminal_finish_app_output()
{
    tcb_t current = get_current_task();
    current->parent_group->parent_task->xtttp_stc->output_lock = true;
}
