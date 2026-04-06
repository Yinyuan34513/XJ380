#include <cpu/msr.h>
#include <errno.h>
#include <pctable/gdt.h>
#include <pctable/idt.h>
#include <proto.hpp>
#include <stdint.h>
#include <syscall/syscall.h>
#include <task/pcb.h>
#include <mm/frame.h>

void init_window_message(WINDOWLSP window, MsgPrcor func)
{
    tcb_t current = window->w_task;
    window->WinMPf = func;

    current->message_queue = queue_init();
    create_message_thread((void *)(XJ380_PRIVATE_MESSAGE_REVERT_ADDRESS + XPSR_OFFEST), 
                        "Window Message Thread", current->parent_group, 
                        strdup(current->str_cwd), current);
}

// Message 线程
void message_thread(tcb_t task)
{
    // 自旋等待输入内容
    uint64_t msg_type, hData, lData;
    MsgPrcor func;
    while (true)
    {
        uint64_t      ret = 0;
        register long r10 __asm__("r10") = (uint64_t)&func;
        register long r8 __asm__("r8")   = (uint64_t)task;
        __asm__ __volatile__("syscall"
                            : "=a"(ret)
                            : "a"(SXAH_MESSAGE_ASK), "D"((uint64_t)&msg_type), "S"((uint64_t)&hData), "d"((uint64_t)&lData), "r"(r10), "r"(r8)
                            : "rcx", "r11", "memory");
        if (ret)
        {
            // 发送message
            func(msg_type, hData, lData);
            continue;
        }
        register long sleep_ms __asm__("rdi") = 1;
        __asm__ __volatile__("syscall" : : "a"(XAPI_SLEEP), "r"(sleep_ms) : "rcx", "r11", "memory");
    }
}

uint64_t message_ask(uint64_t msg_type_p, uint64_t hdatap, uint64_t ldatap, uint64_t funcp, uint64_t taskp)
{
    uint64_t *msg_type  = (uint64_t *)msg_type_p;
    uint64_t *hData     = (uint64_t *)hdatap;
    uint64_t *lData     = (uint64_t *)ldatap;
    MsgPrcor *func      = (MsgPrcor *)funcp;
    tcb_t     current   = (tcb_t)taskp;

    if (current == NULL || current->message_queue == NULL) return 0;

    if (current->message_queue->size > 0)
    {
        // 有待发送的消息，寻找第一个
        lock_node *qc = current->message_queue->head;
        MessageInfoFormat *mif = (MessageInfoFormat *)qc->data;
        *func     = mif->WinMpf;
        *msg_type = mif->msg_type;
        *hData    = mif->hData;
        *lData    = mif->lData;
        // 从队列中去掉，释放资源
        free(mif);
        queue_remove_at(current->message_queue, qc->index);
        return 1;
    }
    return 0;
}

void do_message(uint64_t msg_type, uint64_t hData, uint64_t lData, MsgPrcor WinMpf, tcb_t ftask)
{
    tcb_t current = ftask;

    // 看看需不需要getch?
    if (current->parent_group->xtttp_stc->wait_for_getch && msg_type == MSG_CHAR)
    {
        current->parent_group->xtttp_stc->char_for_getch = lData;
        current->parent_group->xtttp_stc->wait_for_getch = false;
    }

    // 检查是否有注册的信号处理函数
    if (WinMpf != NULL && ftask->message_queue != NULL)
    {
        // 将内容加入队列
        MessageInfoFormat *mif = (MessageInfoFormat *)malloc(sizeof(MessageInfoFormat));
        mif->WinMpf = WinMpf;
        mif->msg_type = msg_type;
        mif->hData = hData;
        mif->lData = lData;
        queue_enqueue(ftask->message_queue, mif);
    }
    // 空的就不用管了
}

void init_message()
{
    size_t message_thread_size = (size_t)((uintptr_t)message_ask - (uintptr_t)message_thread);
    if (message_thread_size == 0 || message_thread_size > (PAGE_SIZE - XPSR_OFFEST))
    {
        write_serial_fmt("XJ380 Message Initialize Failed. Invalid Stub Size: %d\n", message_thread_size);
        return;
    }

    page_map_range_to_random(get_current_directory(), 
                             XJ380_PRIVATE_MESSAGE_REVERT_ADDRESS, 1, 
                             PTE_PRESENT | PTE_WRITEABLE | PTE_USER);
                             
    memcpy((void *)(XJ380_PRIVATE_MESSAGE_REVERT_ADDRESS + XPSR_OFFEST), 
           (void *)message_thread, message_thread_size);

    write_serial_fmt("XJ380 Message Initialize Success.\n");
}
