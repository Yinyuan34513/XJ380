#include "../../include/include.h"

struct IOApicMap_TYPE
{
    UINT8  *IOApicIndexAddr;
    UINT32 *IOApicDataAddr;
    UINT32 *IOApicEoiAddr;
} IOApicMap;


/** 
 * 
 * @brief 检测APIC是否存在
 * 
 * @return 0=NotSupport 1=APIC 2=x2APIC
 * 
 */
UINT8 check_apic() {
    UINT32  cpuid_eax, cpuid_ebx, cpuid_ecx, cpuid_edx;
    asm_cpuid(1, 0, &cpuid_eax, &cpuid_ebx, &cpuid_ecx, &cpuid_edx);
    if (cpuid_ecx & CPUID_ECX_x2APIC) {
        // 支持x2APIC
        return 2;
    } else if (cpuid_edx & CPUID_EDX_APIC) {
        // 支持xAPIC
        return 1;
    }
    return 0;
}

uint64_t IOApicReadRTE(UINT8 index)
{
    // 由于处理器的乱序执行的问题，需要加入内存屏障以保证结果的正确性。
    uint64_t ret;
    // 先读取高32bit
    *IOApicMap.IOApicIndexAddr = index + 1;
    io_mfence();

    ret = *IOApicMap.IOApicDataAddr;
    ret <<= 32;
    io_mfence();

    // 读取低32bit
    *IOApicMap.IOApicIndexAddr = index;
    io_mfence();
    ret |= *IOApicMap.IOApicDataAddr;
    io_mfence();

    return ret;
}

void IOApicWriteRTE(UINT8 index, UINT64 value)
{
    // 先写低32位
    *IOApicMap.IOApicIndexAddr = index;
    io_mfence();

    *IOApicMap.IOApicDataAddr  = value & 0xffffffff;
    io_mfence();
    // 再写高32位
    value >>= 32;
    io_mfence();
    *IOApicMap.IOApicIndexAddr = index + 1;
    io_mfence();
    *IOApicMap.IOApicDataAddr  = value & 0xffffffff;
    io_mfence();
}

void init_IOApic(UINT64 MADT) {
    ACPI_TABLE_HEADER      *MadtHeader           = (ACPI_TABLE_HEADER *)(MADT);             // 获取表头
    MADT_FLAG              *MadtFlag             = (MADT_FLAG *)(MadtHeader + 1);           // 获取Flag
    UINT8                  *MadtEntryPointer     = (UINT8 *)(MadtFlag + 1);                 // 获取表开头
    UINT64                  ioApicAddr           = 0;                                       // APIC寄存器地址

    // 遍历所有表入口
    for (/* PASS */; MadtEntryPointer <= MadtEntryPointer + MadtHeader->length; /* PASS */) {
        MADT_ENTRY_FLAG    *MadtEntryFlagPointer = (MADT_ENTRY_FLAG *)(MadtEntryPointer);   // 获取入口类型
        if (MadtEntryFlagPointer->EntryType == 1) {
            MADT_IO_APIC_FLAG *IOApicAddrEntry = (MADT_IO_APIC_FLAG *)(MadtEntryFlagPointer + 1);
            // 如果定义了就用这个（虽然这个判断看起来很若至但考虑到有些良心厂商emmmmm以防万一）
            if (IOApicAddrEntry->IOApicAddr) {
                ioApicAddr = IOApicAddrEntry->IOApicAddr;
            }
        } else {
            switch (MadtEntryFlagPointer->EntryType)
            {
            case 0:
                MadtEntryPointer += sizeof(MADT_LOCAL_APIC_FLAG);
                break;
            
            case 1:
                MadtEntryPointer += sizeof(MADT_IO_APIC_FLAG);
                break;
            
            case 2:
                MadtEntryPointer += sizeof(MADT_IO_APIC_InterScrOverride);
                break;
            
            case 3:
                MadtEntryPointer += sizeof(MADT_IO_APIC_NMIScr);
                break;
            
            case 4:
                MadtEntryPointer += sizeof(MADT_LOCAL_APIC_NMI);
                break;
            
            case 5:
                MadtEntryPointer += sizeof(MADT_LOCAL_APIC_AddrOverride);
                break;
            
            case 9:
                MadtEntryPointer += sizeof(MADT_LOCAL_x2APIC);
                break;
            
            default:
                break;
            }
        }
    }
    write_serial_string("IO APIC Address Get Success.\n");

    IOApicMap.IOApicIndexAddr        = (UINT8 *)(ioApicAddr);
    IOApicMap.IOApicDataAddr         = (UINT32 *)(ioApicAddr + 0x10);
    IOApicMap.IOApicEoiAddr          = (UINT32 *)(ioApicAddr + 0x40);

    // 设置IO APIC ID 为0x0f000000
    *IOApicMap.IOApicIndexAddr = 0x00;
    io_mfence();
    *IOApicMap.IOApicDataAddr  = 0x0f000000;
    io_mfence();

    io_mfence();

    write_serial_string("IO APIC ID Set Success.\n");

    // 获取IO APIC Version
    *IOApicMap.IOApicIndexAddr = 0x01;
    io_mfence();
    //UINT8 IOApicVersion = *apic_ioapic_map.virtual_data_addr & 0xff;

    // 初始化RTE表项，将所有RTE表项屏蔽
    for (int i = 0x10; i < 0x40; i += 2)
    {
        // 以0x20为起始中断向量号，初始化RTE
        IOApicWriteRTE(i, 0x10020 + ((i - 0x10) >> 1));
    }
    write_serial_string("RTE Initialize Success.\n");
}

/** 
 * 
 * @brief 初始化(Local) x2APIC
 * 
 * @param MADT MADT表地址
 * 
 */
void init_x2Apic(UINT64 MADT) {
    ACPI_TABLE_HEADER      *MadtHeader           = (ACPI_TABLE_HEADER *)(MADT);             // 获取表头
    MADT_FLAG              *MadtFlag             = (MADT_FLAG *)(MadtHeader + 1);           // 获取Flag
    UINT8                  *MadtEntryPointer     = (UINT8 *)(MadtFlag + 1);                 // 获取表开头
    UINT64                  LocalApicAddr        = MadtFlag->LocalApicAddr;                 // APIC寄存器地址

    // 遍历所有表入口
    for (/* PASS */; MadtEntryPointer <= MadtEntryPointer + MadtHeader->length; /* PASS */) {
        MADT_ENTRY_FLAG    *MadtEntryFlagPointer = (MADT_ENTRY_FLAG *)(MadtEntryPointer);   // 获取入口类型
        if (MadtEntryFlagPointer->EntryType == 5) {
            MADT_LOCAL_APIC_AddrOverride *LApicAddrEntry = (MADT_LOCAL_APIC_AddrOverride *)(MadtEntryFlagPointer + 1);
            // 如果定义了就用这个（虽然这个判断看起来很若至但考虑到有些良心厂商emmmmm以防万一）
            if (LApicAddrEntry->LocalApicAddr) {
                LocalApicAddr = LApicAddrEntry->LocalApicAddr;
            }
        } else {
            switch (MadtEntryFlagPointer->EntryType)
            {
            case 0:
                MadtEntryPointer += sizeof(MADT_LOCAL_APIC_FLAG);
                break;
            
            case 1:
                MadtEntryPointer += sizeof(MADT_IO_APIC_FLAG);
                break;
            
            case 2:
                MadtEntryPointer += sizeof(MADT_IO_APIC_InterScrOverride);
                break;
            
            case 3:
                MadtEntryPointer += sizeof(MADT_IO_APIC_NMIScr);
                break;
            
            case 4:
                MadtEntryPointer += sizeof(MADT_LOCAL_APIC_NMI);
                break;
            
            case 5:
                MadtEntryPointer += sizeof(MADT_LOCAL_APIC_AddrOverride);
                break;
            
            case 9:
                MadtEntryPointer += sizeof(MADT_LOCAL_x2APIC);
                break;
            
            default:
                break;
            }
        }
    }
    write_serial_string("Local APIC Address Get Success.\n");

    // 启用xAPIC和x2APIC
    UINT32 ApicMsrEax, ApicMsrEdx;
    GetMSR(IA32_APIC_BASE_MSR, &ApicMsrEax, &ApicMsrEdx);
    ApicMsrEax |= IA32_APIC_BASE_MSR_ENABLE;    // 0000 0000 0000 0000 0000 1000 0000 0000 启用APIC
    ApicMsrEax |= IA32_APIC_BASE_MSR_X2;        // 0000 0000 0000 0000 0000 0100 0000 0000 启用x2APIC
    SetMSR(IA32_APIC_BASE_MSR,  ApicMsrEax,  ApicMsrEdx);
    GetMSR(IA32_APIC_BASE_MSR, &ApicMsrEax, &ApicMsrEdx);
    // 检测是否启用
    if (ApicMsrEax & 0xC00) 
        write_serial_string("x2APIC Enabled Success.\n");
    
    // 设置SVR寄存器，启用Local APIC，禁止EOI广播
    GetMSR(IA32_APIC_SVR_MSR, &ApicMsrEax, &ApicMsrEdx);
    ApicMsrEax |= IA32_APIC_SVR_MSR_LAPIC;      // 0000 0000 0000 0000 0000 0001 0000 0000 启用LocalAPIC
    //ApicMsrEax |= IA32_APIC_SVR_MSR_DISEOI;   // 0000 0000 0000 0000 0001 0000 0000 0000 禁用EOI广播
    SetMSR(IA32_APIC_SVR_MSR,  ApicMsrEax,  ApicMsrEdx);
    GetMSR(IA32_APIC_SVR_MSR, &ApicMsrEax, &ApicMsrEdx);
    // 检测是否成功设置
    if (ApicMsrEax & 0x100) 
        write_serial_string("Local APIC Enabled Success.\n");
    if (ApicMsrEax & 0x1000) 
        write_serial_string("EOI Disabled Success.\n");     // 这个如果不输出事正常情况

    // 关掉LVT
    SetMSR(IA32_APIC_LVT_TIMER,     0x10000, 0x00);
    SetMSR(IA32_APIC_LVT_THERMAL,   0x10000, 0x00);
    SetMSR(IA32_APIC_LVT_PERFOR_MON,0x10000, 0x00);
    SetMSR(IA32_APIC_LVT_LINT0,     0x10000, 0x00);
    SetMSR(IA32_APIC_LVT_LINT1,     0x10000, 0x00);
    SetMSR(IA32_APIC_LVT_ERROR,     0x10000, 0x00);

    *(UINT32 *)(LocalApicAddr + LOCAL_APIC_OFFSET_Local_APIC_LVT_TIMER) = 0x10000;
    io_mfence();
    write_serial_string("Local APIC Initialize Success.\n");
}

/** 
 * 
 * @brief 初始化Local APIC
 * 
 * @param MADT MADT表地址
 * 
 */
void init_apic(UINT64 MADT) {
    write_serial_string("Initializing APIC...\n");

    for (int i = 32; i <= 55; ++i)
        init_idt_desc(i, X86_64_INTR_GATE, interrupt_table[i - 32], RING3);

    // 设置Local APIC中断门
    for (int i = 150; i < 160; ++i)
        init_idt_desc(i, X86_64_INTR_GATE, local_apic_interrupt_table[i - 150], RING3);

    // 屏蔽8259A
    disable_8259A();
    io_mfence();

    if (check_apic() == 2) {
        init_x2Apic(MADT);
    } else if (check_apic() == 1) {
        //
    } else {
        wsod_apic_not_found();
    }

    init_IOApic(MADT);

    write_serial_string("APIC Initialize Success.\n");
}

/**
 * @brief 中断处理函数
 * 
 * @param regs   寄存器
 * @param number 中断号
 */
void do_IRQ(struct X64_REGS *rsp, UINT64 number) {
    // 低于0x80的是外部中断控制器，高于0x80的是Local APIC
    if (number < 0x80 && number >= 32)      // 是外部中断控制器
    {
        IRQ_DESC_TYPE *irq = &interrupt_desc[number - 32];

        // 执行中断上半部处理程序
        if (irq != NULL && irq->handler != NULL) {
            irq->handler(number, irq->parameter, rsp);
        } else {
            //
        }
        // 向中断控制器发送应答消息
        if (irq->controller != NULL && irq->controller->ack != NULL) {
            irq->controller->ack(number);
        } else {
            // 向EOI寄存器写入0x00表示结束中断
            __asm__ volatile ("movq	$0x00,	%%rdx	\n\t"
                                 "movq	$0x00,	%%rax	\n\t"
                                 "movq 	$0x80b,	%%rcx	\n\t"
                                 "wrmsr	\n\t" ::
                                     : "memory");
        }
    }
    else if (number > 0x80)                 // 是Local APIC
    {
        __asm__ volatile ("movq	$0x00,	%%rdx	\n\t"
                                 "movq	$0x00,	%%rax	\n\t"
                                 "movq 	$0x80b,	%%rcx	\n\t"
                                 "wrmsr	\n\t" ::
                                     : "memory");

        {
            IRQ_DESC_TYPE *irq = &SMP_IPI_desc[number - 200];
            if (irq->handler != NULL)
                irq->handler(number, irq->parameter, rsp);
        }
    }
    else
    {
        //
    }
}
