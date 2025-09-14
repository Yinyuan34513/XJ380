#include <stdint.h>
#include <cpu/msr.h>
#include <errno.h>
#include <fs/fs_syscall.h>
#include <pctable/gdt.h>
#include <pctable/idt.h>
#include <proto.hpp>
#include <syscall/syscall.h>
// 信号恢复函数
void signal_revert()
{


    // 通过系统调用返回
    __asm__ __volatile__("syscall"
                         : 
                         : "a"(SXAH_SYSCALL_RETURN)
                         : "rcx", "r11", "memory");
}
void do_signal(uint64_t msg_type, uint64_t data1, uint64_t data2,MsgPrcor WinMpf)
{
    tcb_t current = get_current_task();

    // 检查是否有注册的信号处理函数
    if (WinMpf != NULL) {
        // 保存当前上下文
        current->signal_frame = current->context0;
        current->in_signal_handler = true;

        // 准备信号处理栈
        uint64_t *stack = (uint64_t *)current->signal_handler_stack+KERNEL_STACK_SIZE;
        *(--stack) = data2; // 信号参数
        *(--stack) = data1; // 信号类型
        *(--stack) = msg_type;
        *(--stack) = (uint64_t)signal_revert; // 返回地址

        // 设置新的栈指针和指令指针
        current->context0.rsp = (uint64_t)stack;
        current->context0.rip = (uint64_t)WinMpf;
        switch_to_user_mode();
    }else{
        write_serial_string("WinMpf is NULL!\n");
    }
}

