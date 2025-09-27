#include <cpu/msr.h>
#include <errno.h>
#include <fs/fs_syscall.h>
#include <pctable/gdt.h>
#include <pctable/idt.h>
#include <proto.hpp>
#include <stdint.h>
#include <syscall/syscall.h>
#include <task/pcb.h>

uint64_t sg_rvt_offest;

// 信号恢复函数
void signal_revert()
{
    // 通过系统调用返回
    __asm__ __volatile__("syscall" : : "a"(SXAH_SYSCALL_RETURN) : "rcx", "r11", "memory");
}

void do_signal(uint64_t msg_type, uint64_t data1, uint64_t data2, MsgPrcor WinMpf)
{
    tcb_t current = get_current_task();

    // 检查是否有注册的信号处理函数
    if (WinMpf != NULL)
    {
        // 准备信号处理栈
        create_signal_thread((void *)WinMpf, msg_type, data1, data2, 
                             XJ380_PRIVATE_SIGNAL_REVERT_ADDRESS + sg_rvt_offest, 
                             current->parent_group);
    }
    // 空的就不用管了
}

void signal_end() 
{
    kill_thread(get_current_task()->tid);   // 他紫砂了
    while (true);
}

void init_signal()
{
    sg_rvt_offest = (uint64_t)signal_revert - ((uint64_t)signal_revert & ~(uint64_t)0xFFF);
    page_map_range(get_current_directory(), 
                   XJ380_PRIVATE_SIGNAL_REVERT_ADDRESS, ((uint64_t)signal_revert & ~(uint64_t)0xFFF), 0x2000, 
                   PTE_PRESENT | PTE_WRITEABLE | PTE_USER);

    write_serial_fmt("XJ380 Signal(Message) Initialize Success.\n");
}
