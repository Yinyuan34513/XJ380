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
#include <errno.h>

extern UserInfo *current_user;
extern lock_queue *pcb_group_queue;

void do_xapi_GetSystemVersion(uint64_t str)
{
    char *dst = (char *)str;
    if (dst != NULL)
        memcpy(dst, OS_VERSION, strlen(OS_VERSION) + 1);
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
            strcpy(str, front_p->xtttp_stc->input);
            memset(front_p->xtttp_stc->input, 0, sizeof(front_p->xtttp_stc->input));
            front_p->xtttp_stc->input_lock = false;
            front_p->xtttp_stc->wait_for_input = false;
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
        __asm__ volatile("hlt");
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
    page_directory_t *caller_pagedir = get_current_task()->parent_group->pagedir;
    char  *fpath = vfs_cwd_path_build((char *)path);
    runfile(fpath);
    switch_page_directory(caller_pagedir);
    free(fpath);
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
    if (current_user == NULL) return;

    char setfile_path[256];
    memset(setfile_path, 0, 256);
    strcat(setfile_path, "/users/");
    strcat(setfile_path, current_user->name);
    strcat(setfile_path, "/settings.dat");
    vfs_node_t v = vfs_open(setfile_path);
    if (!v) return;

    SettingsDataFileFormat sdff;
    memset(&sdff, 0, sizeof(sdff));
    size_t read_size = v->size < sizeof(sdff) ? (size_t)v->size : sizeof(sdff);
    if (read_size == 0 || vfs_read(v, &sdff, 0, read_size) == -1)
    {
        vfs_close(v);
        return;
    }

    clock_hour_offset = sdff.ClockHourOffset;

    if (clock_hour_offset < 0)
        clock_hour_offset = 0;

    vfs_close(v);
}

static uint64_t taskmgr_process_memory_bytes(pcb_t process, uint64_t thread_count)
{
    if (process == NULL) return 0;

    uint64_t bytes = process->vma_manager.vm_used;
    bytes += thread_count * (BIG_USER_STACK + KERNEL_STACK_SIZE * 2);
    return bytes;
}

static void taskmgr_copy_name(char *dst, const char *src)
{
    if (dst == NULL) return;
    memset(dst, 0, XAPI_TASK_NAME_LEN);
    if (src == NULL) return;
    strncpy(dst, src, XAPI_TASK_NAME_LEN - 1);
}

static uint64_t taskmgr_count_windows(pcb_t process, uint64_t *thread_count)
{
    if (thread_count != NULL) *thread_count = 0;
    if (process == NULL || process->thread_queue == NULL) return 0;

    uint64_t windows = 0;
    spin_lock(&process->thread_queue->lock);
    queue_foreach(process->thread_queue, node)
    {
        tcb_t thread = (tcb_t)node->data;
        if (thread == NULL) continue;
        windows += thread->window_count;
        if (thread_count != NULL) (*thread_count)++;
    }
    spin_unlock(&process->thread_queue->lock);
    return windows;
}

static void taskmgr_fill_info(XapiTaskInfo *info, pcb_t process, tcb_t thread, uint64_t thread_count,
                              uint64_t window_count, uint64_t memory_bytes)
{
    memset(info, 0, sizeof(XapiTaskInfo));
    info->pid            = process->pid;
    info->ppid           = process->ppid;
    info->tid            = thread != NULL ? thread->tid : 0;
    info->cpu_id         = thread != NULL ? thread->cpu_id : 0;
    info->task_level     = thread != NULL ? thread->task_level : process->task_level;
    info->thread_count   = thread_count;
    info->window_count   = window_count;
    info->memory_bytes   = memory_bytes;
    info->process_status = process->status;
    info->thread_status  = thread != NULL ? thread->status : process->status;
    taskmgr_copy_name(info->process_name, process->name);
    taskmgr_copy_name(info->thread_name, thread != NULL ? thread->name : process->name);
}

uint64_t do_xapi_GetTaskList(uint64_t buffer, uint64_t max_count)
{
    if (buffer == 0 || max_count == 0 || pcb_group_queue == NULL) return 0;

    XapiTaskInfo *out     = (XapiTaskInfo *)buffer;
    uint64_t      written = 0;

    spin_lock(&pcb_group_queue->lock);
    queue_foreach(pcb_group_queue, pnode)
    {
        pcb_t process = (pcb_t)pnode->data;
        if (process == NULL) continue;

        uint64_t thread_count = 0;
        uint64_t window_count = taskmgr_count_windows(process, &thread_count);
        uint64_t memory_bytes = taskmgr_process_memory_bytes(process, thread_count);

        if (process->thread_queue == NULL || thread_count == 0)
        {
            if (written >= max_count) break;
            taskmgr_fill_info(&out[written++], process, NULL, thread_count, window_count, memory_bytes);
            continue;
        }

        spin_lock(&process->thread_queue->lock);
        queue_foreach(process->thread_queue, tnode)
        {
            if (written >= max_count) break;
            tcb_t thread = (tcb_t)tnode->data;
            if (thread == NULL) continue;
            taskmgr_fill_info(&out[written++], process, thread, thread_count, window_count, memory_bytes);
        }
        spin_unlock(&process->thread_queue->lock);

        if (written >= max_count) break;
    }
    spin_unlock(&pcb_group_queue->lock);

    return written;
}

uint64_t do_xapi_KillProcess(uint64_t pid)
{
    if (kernel_group == NULL) return (uint64_t)-ESRCH;

    pcb_t current = get_current_task()->parent_group;
    pcb_t target  = found_pcb((int)pid);
    if (target == NULL || target->status == DEATH || target->status == OUT) return (uint64_t)-ESRCH;
    if (target == kernel_group || target->task_level == TASK_KERNEL_LEVEL) return (uint64_t)-EPERM;
    if (target == current) return (uint64_t)-EPERM;

    kill_proc(target, -1, false);
    return 0;
}
