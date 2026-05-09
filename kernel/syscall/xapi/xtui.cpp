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
#include <mm/uaccess.h>
#include <pci/pci.h>
#include <proto.hpp>
#include <rtc.h>
#include <syscall/pxapi.h>
#include <syscall/xapi_user.h>
#include <task/pcb.h>
#include <ttf.h>
#include <syscall/syscall.h>
#include "../../build_settings.h"
#include <user/runfile.h>
#include <errno.h>

extern UserInfo *current_user;
extern lock_queue *pcb_group_queue;

static constexpr size_t XAPI_RUN_ARG_MAX = 32;

uint64_t do_xapi_GetTime()
{
    tm t;
    time_read(&t);
    return mktime(&t);
}

// p 开头的代表 private
void p_xapi_output_kernel(const char *str)
{
    if (str == NULL) return;

    pcb_t front_p = get_current_task()->parent_group;
    pcb_t caller = front_p;
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
            if (str == NULL) {
                return;
            }

            char  *output    = front_p->xtttp_stc->output;
            size_t  out_cap  = sizeof(front_p->xtttp_stc->output);
            size_t  out_len  = strlen(output);
            size_t  str_len  = strlen(str);
            size_t  copy_len = str_len;

            if (out_len < out_cap - 1)
            {
                size_t avail = out_cap - out_len - 1;
                if (copy_len > avail) copy_len = avail;
                memcpy(output + out_len, str, copy_len);
                output[out_len + copy_len] = '\0';
            }

            front_p->xtttp_stc->output_lock = false;

            while (true)
            {
                if (front_p->xtttp_stc->output_lock) break;
                scheduler_yield();
            }
            memset(front_p->xtttp_stc->output, 0, sizeof(front_p->xtttp_stc->output));
            return;
        }
        front_p = front_p->parent_task;
    }
}

static void p_xapi_free_argv(char **argv)
{
    if (argv == NULL) return;
    for (size_t i = 0; argv[i] != NULL; i++)
    {
        free(argv[i]);
    }
    free(argv);
}

static int p_xapi_copy_argv_from_user(char **user_argv, char ***out)
{
    if (out == NULL) return -EINVAL;
    *out = NULL;
    if (user_argv == NULL) return 0;

    page_directory_t *pagedir = xapi_current_pagedir();
    if (pagedir == NULL) return -EFAULT;

    char **argv = (char **)calloc(XAPI_RUN_ARG_MAX + 1, sizeof(char *));
    if (argv == NULL) return -ENOMEM;

    for (size_t i = 0; i < XAPI_RUN_ARG_MAX; i++)
    {
        char *user_arg = NULL;
        if (!copy_from_user_pagedir(pagedir, &user_arg, &user_argv[i], sizeof(user_arg)))
        {
            p_xapi_free_argv(argv);
            return -EFAULT;
        }
        if (user_arg == NULL)
        {
            *out = argv;
            return 0;
        }
        int ret = xapi_copy_string_from_user(&argv[i], user_arg, XAPI_USER_STRING_MAX);
        if (ret < 0)
        {
            p_xapi_free_argv(argv);
            return ret;
        }
    }

    p_xapi_free_argv(argv);
    return -E2BIG;
}

static size_t p_xapi_strnlen(const char *str, size_t max_len)
{
    if (str == NULL) return 0;
    size_t len = 0;
    while (len < max_len && str[len] != '\0') len++;
    return len;
}

void do_xapi_GetSystemVersion(uint64_t str)
{
    if (str == 0) return;
    copy_to_user_pagedir(xapi_current_pagedir(), (void *)str, OS_VERSION, strlen(OS_VERSION) + 1);
}

void do_xapi_GetCurrentUser(uint64_t dst)
{
    if (dst == 0 || current_user == NULL) return;
    xapi_type_UserInfo info;
    memset(&info, 0, sizeof(info));
    strncpy(info.name, current_user->name, sizeof(info.name) - 1);
    info.user_type = current_user->user_type;
    copy_to_user_pagedir(xapi_current_pagedir(), (void *)dst, &info, sizeof(info));
}

void do_xapi_Output(char *str)
{
    char *kstr = NULL;
    if (xapi_copy_string_from_user(&kstr, str, XAPI_USER_STRING_MAX) < 0) return;
    p_xapi_output_kernel(kstr);
    free(kstr);
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
            size_t input_len = p_xapi_strnlen(front_p->xtttp_stc->input, sizeof(front_p->xtttp_stc->input) - 1);
            front_p->xtttp_stc->input[input_len] = '\0';
            copy_to_user_pagedir(xapi_current_pagedir(), str, front_p->xtttp_stc->input, input_len + 1);
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
    p_xapi_output_kernel("\n");
}

void do_xapi_Printline(char *str)
{
    char *kstr = NULL;
    if (xapi_copy_string_from_user(&kstr, str, XAPI_USER_STRING_MAX) < 0) return;
    size_t len = p_xapi_strnlen(kstr, XAPI_USER_STRING_MAX - 2);
    kstr[len] = '\n';
    kstr[len + 1] = '\0';
    p_xapi_output_kernel(kstr);
    free(kstr);
}

void do_xapi_OutputSerial(char *str)
{
    char *kstr = NULL;
    if (xapi_copy_string_from_user(&kstr, str, XAPI_USER_STRING_MAX) < 0) return;
    tcb_t task = get_current_task();
    write_serial_fmt("[xapi-serial task=%s ptr=0x%llx] ",
                     task != NULL ? task->name : "<none>",
                     (unsigned long long)(uint64_t)str);
    write_serial_string(kstr);
    free(kstr);
}

void do_xapi_Printf(char *str)
{
    do_xapi_Output(str);
}

void do_xapi_Sleep(uint64_t ms)
{
    uint64_t nano = ms * 1000000ULL;
    if (ms != 0 && nano / 1000000ULL != ms) nano = (uint64_t)-1;
    scheduler_sleep_ns(nano);
}

void do_xapi_GetTimeX(uint64_t tt)
{
    if (!tt) { return; }
    tm local;
    memset(&local, 0, sizeof(local));
    time_read(&local);
    copy_to_user_pagedir(get_current_task()->parent_group->pagedir, (void *)tt, &local, sizeof(local));
}

extern uint64_t memory_total_size;

uint64_t do_xapi_GetMemorySize()
{
    return memory_total_size / 1024;
}

void do_xapi_Run(char *path)
{
    page_directory_t *caller_pagedir = get_current_task()->parent_group->pagedir;
    char *upath = NULL;
    if (xapi_copy_string_from_user(&upath, path, XAPI_USER_PATH_MAX) < 0) return;
    char  *fpath = vfs_cwd_path_build(upath);
    free(upath);
    if (fpath == NULL) return;
    runfile(fpath);
    switch_page_directory(caller_pagedir);
    free(fpath);
}

uint64_t do_xapi_RunArgs(char *path, char **argv)
{
    page_directory_t *caller_pagedir = get_current_task()->parent_group->pagedir;
    char *upath = NULL;
    int ret = xapi_copy_string_from_user(&upath, path, XAPI_USER_PATH_MAX);
    if (ret < 0) return (uint64_t)ret;

    char *fpath = vfs_cwd_path_build(upath);
    free(upath);
    if (fpath == NULL) return (uint64_t)-ENOENT;

    char **kargv = NULL;
    ret = p_xapi_copy_argv_from_user(argv, &kargv);
    if (ret < 0)
    {
        free(fpath);
        return (uint64_t)ret;
    }
    write_serial_fmt("[busybox-debug] do_xapi_RunArgs fpath=%s\n", fpath);
    if (kargv != NULL)
    {
        for (int i = 0; kargv[i] != NULL; i++)
        {
            write_serial_fmt("[busybox-debug] do_xapi_RunArgs argv[%d]=%s\n", i, kargv[i]);
        }
    }

    ret = create_user_process_from_file(fpath, get_current_task()->parent_group, kargv);
    switch_page_directory(caller_pagedir);
    p_xapi_free_argv(kargv);
    free(fpath);
    return (uint64_t)ret;
}

uint64_t do_xapi_MapMemory(uint64_t ptr, uint64_t size, uint32_t flags)
{
    uint64_t ret = -1;
    if (size == 0 || check_user_overflow(ptr, size)) return ret;
    flags |= PTE_USER;
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

    page_directory_t *pagedir = xapi_current_pagedir();
    if (pagedir == NULL) return 0;

    XapiTaskInfo *out     = (XapiTaskInfo *)buffer;
    uint64_t      written = 0;
    bool          copy_fault = false;

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
            XapiTaskInfo info;
            taskmgr_fill_info(&info, process, NULL, thread_count, window_count, memory_bytes);
            if (!copy_to_user_pagedir(pagedir, &out[written], &info, sizeof(info)))
            {
                copy_fault = true;
                break;
            }
            written++;
            continue;
        }
        if (copy_fault) break;

        spin_lock(&process->thread_queue->lock);
        queue_foreach(process->thread_queue, tnode)
        {
            if (written >= max_count) break;
            tcb_t thread = (tcb_t)tnode->data;
            if (thread == NULL) continue;
            XapiTaskInfo info;
            taskmgr_fill_info(&info, process, thread, thread_count, window_count, memory_bytes);
            if (!copy_to_user_pagedir(pagedir, &out[written], &info, sizeof(info)))
            {
                copy_fault = true;
                break;
            }
            written++;
        }
        spin_unlock(&process->thread_queue->lock);

        if (written >= max_count || copy_fault) break;
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
