#include <ahci/ahci.h>
#include <efi/boot.h>
#include <efi/efi.h>
#include <efi/fbc.h>
#include <font.h>
#include <fs/fatfs/fatfs.h>
#include <fs/partition.h>
#include <fs/vfs/devfs.h>
#include <fs/vfs/sys.h>
#include <fs/vfs/vfs.h>
#include <global_color.h>
#include <graphics/GOP.hpp>
#include <graphics/sheet.h>
#include <graphics/window/window.h>
#include <krlibc.h>
#include <mm/frame.h>
#include <pci/pci.h>
#include <proto.hpp>
#include <rtc.h>
#include <syscall/pxapi.h>
#include <task/pcb.h>
#include <ttf.h>
#include <syscall/syscall.h>
#include "../../build_settings.h"
#include <user/runfile.h>

extern UserInfo *current_user;

void do_xapi_GetSystemVersion(uint64_t str)
{
    char *dst = (char *)str;
    if (dst != NULL)
        memcpy(dst, OS_VERSION, strlen(OS_VERSION));
}

uint64_t do_xapi_GetTime()
{
    tm t;
    time_read(&t);
    return mktime(&t);
}

void do_xapi_GetCurrentUser(uint64_t dst)
{
    xapi_type_UserInfo *dest = (xapi_type_UserInfo *)dst;
    memcpy(dest->name, current_user->name, strlen(current_user->name));
    dest->user_type = current_user->user_type;
}

void do_xapi_Output(char *str)
{
    pcb_t front_p = get_current_task()->parent_group;
    // // while(!front_p->parent_task->co)cpu_relax();
    
    while (true)
    {
        if (front_p == NULL || front_p == kernel_group)
        {
            pr_warn("A program call xapi_Output, but cannot found parent terminal. \n");
            return;
        }
        if (front_p->xtttp_stc->is_shell)
        {
            // found it!
            strcat(front_p->xtttp_stc->output, str); // 如果还有怎么办? // ? 怎么会还有呢，这就是一次性输出完的啊
            front_p->xtttp_stc->output_lock = false;
            // 等待命令行刷新
            while (true)
            {
                if (front_p->xtttp_stc->output_lock) break;
                scheduler_yield();
            }
            memset(front_p->xtttp_stc->output, 0, (strlen(front_p->xtttp_stc->output) + 1) * sizeof(uint8_t));
            return;
        }
        front_p = front_p->parent_task;
    }
}

void do_xapi_Input(char *str)
{
    pcb_t front_p = get_current_task()->parent_group;
    while (true)
    {
        if (front_p == NULL || front_p == kernel_group)
        {
            pr_warn("A program call xapi_Input, but cannot found parent terminal. \n");
            return;
        }
        if (front_p->xtttp_stc->is_shell)
        {
            // 等待命令行输入
            front_p->xtttp_stc->wait_for_input = true;
            while (true)
            {
                if (front_p->xtttp_stc->input_lock) break;
                scheduler_yield();
            }
            // found it!
            strcat(str, "\0");
            strcpy(str, front_p->xtttp_stc->input);
            front_p->xtttp_stc->input_lock = false;
            memset(front_p->xtttp_stc->output, 0, (strlen(front_p->xtttp_stc->input) + 1) * sizeof(uint8_t));
            return;
        }
        front_p = front_p->parent_task;
    }
}

char do_xapi_Getch()
{
    pcb_t front_p = get_current_task()->parent_group;
    while (true)
    {
        if (front_p == NULL || front_p == kernel_group)
        {
            pr_warn("A program call xapi_Getch, but cannot found parent terminal. \n");
            return 0;
        }
        if (front_p->xtttp_stc->is_shell)
        {
            // 等待命令行输入
            front_p->xtttp_stc->wait_for_getch = true;
            while (true)
            {
                if (!front_p->xtttp_stc->wait_for_getch) break;
                scheduler_yield();
            }
            // found it!
            return front_p->xtttp_stc->char_for_getch;
        }
        front_p = front_p->parent_task;
    }
}

void do_xapi_Endline()
{
    do_xapi_Output("\n");
}

void do_xapi_Printline(char *str)
{
    do_xapi_Output(strcat(str, "\n"));
}

void do_xapi_OutputSerial(char *str)
{
    write_serial_string(str);
}

void do_xapi_Printf(char *str)
{
    do_xapi_Output(str);
}

void do_xapi_Sleep(uint64_t ms)
{
    open_interrupt;
    uint64_t old_t = nanoTime();
    while (nanoTime() - old_t <= ms * 1000000)
    {
        __asm__ volatile("pause");
    }
    close_interrupt;
}

void do_xapi_GetTimeX(uint64_t tt)
{
    if (!tt) { return; }
    tm *time_s = (tm *)tt;
    time_read(time_s);
}

extern uint64_t memory_total_size;

uint64_t do_xapi_GetMemorySize()
{
    return memory_total_size / 1024;
}

void do_xapi_Run(char *path)
{
    runfile(path);
}

uint64_t do_xapi_MapMemory(uint64_t ptr, uint64_t size, uint32_t flags)
{
    uint64_t ret = -1;
    if (ptr == NULL)
    {
        ret = page_alloc_random(get_current_task()->parent_group->pagedir, size, flags);
    }
    else
    {
        page_map_range_to_random(get_current_task()->parent_group->pagedir, ptr, size, flags);
        ret = ptr;
    }
    return ret;
}

extern int clock_hour_offset;

void do_xapi_FlushTime()
{
    char setfile_path[256];
    memset(setfile_path, 0, 256);
    strcat(setfile_path, "/Users/");
    strcat(setfile_path, current_user->name);
    strcat(setfile_path, "/settings.dat");
    vfs_node_t v = vfs_open(setfile_path);
    if (!v) return;
    SettingsDataFileFormat *sdff = (SettingsDataFileFormat *)malloc(v->size);
    if (!sdff) return;
    vfs_read(v, sdff, 0, v->size);

    clock_hour_offset = sdff->ClockHourOffset;

    if (clock_hour_offset < 0)
        clock_hour_offset = 0;

    vfs_close(v);
    free(sdff);
}
