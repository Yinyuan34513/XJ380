#include "../../include/include.h"

int sys_exit(int status)
{
    return do_exit(status);
}

int sys_fork()
{
    struct X64_REGS *regs = (struct X64_REGS *)current_task->thread->rsp0 - 1;
    return do_fork(regs, 0, regs->rsp, 0);
}

int sys_execve(struct X64_REGS_SYSCALL *regs_arg)
{
    char *pathname = NULL;
    long pathlen = 0;
    long error = 0;
    struct X64_REGS_SYSCALL *regs = (struct X64_REGS_SYSCALL *)current_task->thread->rsp0 - 1;

    pathname = (char *)malloc(PAGE_SIZE);
    if (pathname == NULL)
        return -1;
    xmemset(pathname, 0, PAGE_SIZE);
    pathlen = strlen((char *)regs_arg->rdi);
    if (pathlen <= 0)
    {
        free(pathname);
        return -1;
    }
    else if (pathlen >= PAGE_SIZE)
    {
        free(pathname);
        return -1;
    }
    strncpy(pathname, (const char *)regs_arg->rdi, pathlen);

    error = do_execve(regs, pathname, (char **)regs_arg->rsi, (char **)regs_arg->rdx);

    free(pathname);
    return error;
}

uint64_t sys_wait4(unsigned long pid, int *status, int options, void *rusage)
{
    long retval = 0;
    struct task_struct *child = NULL;
    struct task_struct *tsk = NULL;

    for (tsk = &init_task_union.task; tsk->next != &init_task_union.task; tsk = tsk->next)
    {
        if (tsk->next->pid == pid)
        {
            child = tsk->next;
            break;
        }
    }

    if (child == NULL)
        return -1;
    if (options != 0)
        return -1;

    if (child->state == TASK_ZOMBIE)
    {
        xmemcpy(status, &child->exit_code, sizeof(int));
        tsk->next = child->next;
        return retval;
    }

    interruptible_sleep_on(&current_task->wait_childexit);

    xmemcpy(status, &child->exit_code, sizeof(long));
    tsk->next = child->next;
    return retval;
}
