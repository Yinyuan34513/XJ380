#include <cpu/msr.h>
#include <errno.h>
#include <fs/fs_syscall.h>
#include <pctable/gdt.h>
#include <pctable/idt.h>
#include <proto.hpp>
#include <stdint.h>
#include <syscall/syscall.h>
#include <task/pcb.h>
#include <mm/frame.h>

uint64_t sg_rvt_offest;

// 信号恢复函数
void signal_revert()
{
    // 通过系统调用返回
    __asm__ __volatile__("syscall" : : "a"(SXAH_SYSCALL_RETURN) : "rcx", "r11", "memory");
}

void do_signal(uint64_t msg_type, uint64_t data1, uint64_t data2, MsgPrcor WinMpf, tcb_t ftask)
{
    tcb_t current = ftask;

    // 检查是否有注册的信号处理函数
    if (WinMpf != NULL && !current->parent_group->sigprci)
    {
        current->parent_group->sigprci = true;
        // 准备信号处理栈
        create_signal_thread((void *)WinMpf, msg_type, data1, data2, 
                             XJ380_PRIVATE_SIGNAL_REVERT_ADDRESS + XPSR_OFFEST, 
                             current->parent_group);
    }
    // 空的就不用管了
}

void signal_end() 
{
    get_current_task()->parent_group->sigprci = false;
    kill_thread(get_current_task()->tid, get_current_task()->parent_group); // 他紫砂了
    scheduler_yield();
    while (true)
    {
        __asm__ volatile("pause\n\t");
    }
}

void init_signal()
{
    page_map_range_to_random(get_current_directory(), 
                             XJ380_PRIVATE_SIGNAL_REVERT_ADDRESS, 1, 
                             PTE_PRESENT | PTE_WRITEABLE | PTE_USER);
                             
    memcpy((void *)(XJ380_PRIVATE_SIGNAL_REVERT_ADDRESS + XPSR_OFFEST), 
           (void *)signal_revert, 0xff);   // 我也不知道这个会有多大，就这样吧先

    write_serial_fmt("XJ380 Signal(Message) Initialize Success.\n");
}
