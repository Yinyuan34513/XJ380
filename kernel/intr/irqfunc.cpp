#include "../../include/include.h"

/**
 * @brief 中断注册函数
 *
 * @param irq_num 中断向量号
 * @param arg 传递给中断安装接口的参数
 * @param handler 中断处理函数
 * @param paramater 中断处理函数的参数
 * @param controller 中断控制器结构
 * @param irq_name 中断名
 * @return int
 */
int irq_register(UINT64 irq_num, void *arg, void (*handler)(UINT64 irq_num, UINT64 parameter, struct X64_REGS *regs), UINT64 paramater, HARDWARE_INTR_CTRL *controller, char *irq_name)
{
    // 由于为I/O APIC分配的中断向量号是从32开始的，因此要减去32才是对应的interrupt_desc的元素
    IRQ_DESC_TYPE *p = NULL;
    if (irq_num >= 32 && irq_num < 0x80)
        p = &interrupt_desc[irq_num - 32];
    else if (irq_num >= 150 && irq_num < 200)
        p = &local_apic_interrupt_desc[irq_num - 150];
    else
    {
        write_serial_string("[irq.c:188]Invaild IRQ.");
        return -1;
    }
    p->controller = controller;
    if (p->irq_name == NULL)
    {
        int namelen = strlen(irq_name) + 1;
        p->irq_name = (char *)malloc(namelen);
        xmemset(p->irq_name, 0, namelen);
        strncpy(p->irq_name, irq_name, namelen);
    }

    p->parameter = paramater;
    p->flags = 0;
    p->handler = handler;

    p->controller->install(irq_num, arg);
    p->controller->enable(irq_num);

    return 0;
}

/**
 * @brief 中断注销函数
 *
 * @param irq_num 中断向量号
 * @return int
 */
int irq_unregister(UINT64 irq_num)
{
    IRQ_DESC_TYPE *p = &interrupt_desc[irq_num - 32];
    p->controller->disable(irq_num);
    p->controller->uninstall(irq_num);

    p->controller = NULL;
    if (p->irq_name)
        free(p->irq_name);
    p->irq_name = NULL;
    p->parameter = 0;
    p->flags = 0;
    p->handler = NULL;

    return 0;
}

void ipi_send_IPI(UINT32 dest_mode, UINT32 deliver_status, UINT32 level, UINT32 trigger,
                  UINT32 vector, UINT32 deliver_mode, UINT32 dest_shorthand, bool apic_type, 
                  UINT32 destination)
{
    struct INT_CMD_REG icr_entry;
    icr_entry.dest_mode = dest_mode;
    icr_entry.deliver_status = deliver_status;
    icr_entry.res_1 = 0;
    icr_entry.level = level;
    icr_entry.trigger = trigger;
    icr_entry.res_2 = 0;
    icr_entry.res_3 = 0;

    icr_entry.vector = vector;
    icr_entry.deliver_mode = deliver_mode;
    icr_entry.dest_shorthand = dest_shorthand;

    icr_entry.destination.x2apic_destination = destination;
    SetMSR(0x830, (*(unsigned long *)&icr_entry & 0xffffffff), (*(unsigned long *)&icr_entry << 32)); // 发送ipi
}

int ipi_regiserIPI(UINT64 irq_num, void *arg,
                   void (*handler)(UINT64 irq_num, UINT64 param, struct X64_REGS *regs),
                   UINT64 param, HARDWARE_INTR_CTRL *controller, char *irq_name)
{
    IRQ_DESC_TYPE *p = &SMP_IPI_desc[irq_num - 200];
    p->controller = NULL;
    p->irq_name = irq_name;
    p->parameter = param;
    p->flags = 0;
    p->handler = handler;
    return 0;
}

/**
 * @brief 初始化中断模块
 */
void init_irq(UINT64 MADT)
{
    xmemset((void *)interrupt_desc,             0, sizeof(IRQ_DESC_TYPE) * IRQ_NUM);

    init_apic(MADT);
}
