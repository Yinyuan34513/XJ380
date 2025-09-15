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

void do_signal(uint64_t msg_type, uint64_t data1, uint64_t data2, MsgPrcor WinMpf)
{
    tcb_t current = get_current_task();

    // 检查是否有注册的信号处理函数
    if (WinMpf != NULL) {
        // 准备信号处理栈
    } else {
        write_serial_string("[Warning] Window Message Process Function is NULL!\n");
    }
}

