#include <cpu/msr.h>
#include <errno.h>
#include <pctable/gdt.h>
#include <pctable/idt.h>
#include <proto.hpp>
#include <stdint.h>
#include <syscall/syscall.h>
#include <task/pcb.h>
#include <mm/frame.h>

// 信号恢复函数
void message_revert()
{
    // 通过系统调用返回
    __asm__ __volatile__("syscall" : : "a"(SXAH_SYSCALL_RETURN) : "rcx", "r11", "memory");
}

void do_message(uint64_t msg_type, uint64_t data1, uint64_t data2, MsgPrcor WinMpf, tcb_t ftask)
{
    tcb_t current = ftask;

    // 看看需不需要getch?
    if (current->parent_group->xtttp_stc->wait_for_getch && msg_type == MSG_CHAR)
    {
        current->parent_group->xtttp_stc->char_for_getch = data2;
        current->parent_group->xtttp_stc->wait_for_getch = false;
    }

    // 检查是否有注册的信号处理函数
    if (WinMpf != NULL && !current->parent_group->msgprci)
    {
        current->parent_group->msgprci = true;
        // 准备信号处理栈
        create_signal_thread((void *)WinMpf, msg_type, data1, data2, 
                             XJ380_PRIVATE_MESSAGE_REVERT_ADDRESS + XPSR_OFFEST, 
                             current->parent_group);
    }
    // 空的就不用管了
}

void message_end() 
{
    get_current_task()->parent_group->msgprci = false;
    kill_thread(get_current_task()); // 他紫砂了
    while (true)
    {
        scheduler_yield();
        __asm__ volatile("pause\n\t");
    }
}

void init_signal()
{
    page_map_range_to_random(get_current_directory(), 
                             XJ380_PRIVATE_MESSAGE_REVERT_ADDRESS, 1, 
                             PTE_PRESENT | PTE_WRITEABLE | PTE_USER);
                             
    memcpy((void *)(XJ380_PRIVATE_MESSAGE_REVERT_ADDRESS + XPSR_OFFEST), 
           (void *)message_revert, 0xff);   // 我也不知道这个会有多大，就这样吧先

    write_serial_fmt("XJ380 Signal(Message) Initialize Success.\n");
}
