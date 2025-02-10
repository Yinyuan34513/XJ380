#include "../../include/include.h"

int sys_exit(int status)
{
    return do_exit(status);
}

int sys_fork(struct X64_REGS_SYSCALL *regs)
{
    return do_fork((struct X64_REGS *)regs, 0, regs->rsp, 0);
}

int sys_vfork(struct X64_REGS_SYSCALL *regs)
{
    return do_fork((struct X64_REGS *)regs, CLONE_VM, regs->rsp, 0);
}

int sys_execve(struct X64_REGS_SYSCALL *regs)
{
    return do_execve(regs, (const char *)regs->rdi, (char **)regs->rsi, (char **)regs->rdx);
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
