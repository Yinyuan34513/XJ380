#include "../../include/include.h"

APIC_INFO ApicInfo;

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

void read_lapic_reg(UINT32 offest, UINT32 *lo, UINT32 *hi) {
    if (ApicInfo.x2Apic) {
        // MSR
        GetMSR((0x800 + (offest >> 4)), lo, hi);
    } else {
        // MMIO
        *(volatile UINT32 *)(ApicInfo.LocalApicAddr + offest) = (UINT32)(*lo);
    }
}

void write_lapic_reg(UINT32 offest, UINT32 lo, UINT32 hi) {
    if (ApicInfo.x2Apic) {
        // MSR
        SetMSR((0x800 + (offest >> 4)), lo, hi);
    } else {
        // MMIO
        *(volatile UINT32 *)(ApicInfo.LocalApicAddr + offest) = (UINT32)(lo);
    }
}

void init_IOApic(UINT64 MADT) {
    ACPI_TABLE_HEADER      *MadtHeader           = (ACPI_TABLE_HEADER *)(MADT);             // 获取表头
    MADT_FLAG              *MadtFlag             = (MADT_FLAG *)(MadtHeader + 1);           // 获取Flag
    UINT8                  *MadtEntryPointer     = (UINT8 *)(MadtFlag + 1);                 // 获取表开头

    ApicInfo.ioApicAddr                          = 0xfec00000;                              // APIC寄存器地址

    // 遍历所有表入口
    write_serial_string("Seraching IO APIC...\n");
    write_serial_string("MadtLength: ");
    write_serial_dec(MadtHeader->length);
    write_serial_string("\n");
    write_serial_string("MadtAddress: ");
    write_serial_hex(MADT);
    write_serial_string("\n");
    write_serial_string("MadtFootAddress: ");
    write_serial_hex((UINT64)MadtEntryPointer + MadtHeader->length);
    write_serial_string("\n");
    // ??????????????????????????????????????????????????????
    for (int i = 0; i <= MadtHeader->length; /* PASS */) {
        MADT_ENTRY_FLAG    *MadtEntryFlagPointer = (MADT_ENTRY_FLAG *)(MadtEntryPointer);   // 获取入口类型

        write_serial_string("MadtEntryType ");
        write_serial_dec(MadtEntryFlagPointer->EntryType);
        write_serial_string("\n");
        write_serial_string("MadtEntryLength ");
        write_serial_dec(MadtEntryFlagPointer->length);
        write_serial_string("\n");
        write_serial_string("MadtEntryAddress ");
        write_serial_hex((UINT64)(MadtEntryPointer));
        write_serial_string("\n");

        if (MadtEntryFlagPointer->EntryType == 1) {
            MADT_IO_APIC_FLAG *IOApicAddrEntry = (MADT_IO_APIC_FLAG *)(MadtEntryFlagPointer + 1);
            // 如果定义了就用这个（虽然这个判断看起来很若至但考虑到有些良心厂商emmmmm以防万一）
            if (IOApicAddrEntry->IOApicAddr) {
                ApicInfo.ioApicAddr = IOApicAddrEntry->IOApicAddr;
            }
        }

        MadtEntryPointer += MadtEntryFlagPointer->length;
        i                += MadtEntryFlagPointer->length;
    }
    write_serial_string("IO APIC Address Get Success.\n");
    write_serial_string("IOApicAddress: ");
    write_serial_hex((UINT64)(ApicInfo.ioApicAddr));
    write_serial_string("\n");
}

/** 
 * 
 * @brief 初始化(Local) x2APIC
 * 
 * @param MADT MADT表地址
 * 
 */
void init_lApic(UINT64 MADT) {
    ACPI_TABLE_HEADER      *MadtHeader           = (ACPI_TABLE_HEADER *)(MADT);             // 获取表头
    MADT_FLAG              *MadtFlag             = (MADT_FLAG *)(MadtHeader + 1);           // 获取Flag
    UINT8                  *MadtEntryPointer     = (UINT8 *)(MadtFlag + 1);                 // 获取表开头
    
    ApicInfo.LocalApicAddr                       = MadtFlag->LocalApicAddr;                 // APIC寄存器地址

    // 遍历所有表入口
    write_serial_string("Seraching Local APIC...\n");
    write_serial_string("MadtLength: ");
    write_serial_dec(MadtHeader->length);
    write_serial_string("\n");
    write_serial_string("MadtAddress: ");
    write_serial_hex(MADT);
    write_serial_string("\n");
    write_serial_string("MadtFootAddress: ");
    write_serial_hex((UINT64)MadtEntryPointer + MadtHeader->length);
    write_serial_string("\n");
    // ??????????????????????????????????????????????????????
    for (int i = 0; i <= MadtHeader->length; /* PASS */) {
        MADT_ENTRY_FLAG    *MadtEntryFlagPointer = (MADT_ENTRY_FLAG *)(MadtEntryPointer);   // 获取入口类型

        write_serial_string("MadtEntryType ");
        write_serial_dec(MadtEntryFlagPointer->EntryType);
        write_serial_string("\n");
        write_serial_string("MadtEntryLength ");
        write_serial_dec(MadtEntryFlagPointer->length);
        write_serial_string("\n");
        write_serial_string("MadtEntryAddress ");
        write_serial_hex((UINT64)(MadtEntryPointer));
        write_serial_string("\n");

        if (MadtEntryFlagPointer->EntryType == 5) {
            MADT_LOCAL_APIC_AddrOverride *LocalApicAddrEntry = (MADT_LOCAL_APIC_AddrOverride *)(MadtEntryFlagPointer + 1);
            // 如果定义了就用这个（虽然这个判断看起来很若至但考虑到有些良心厂商emmmmm以防万一）
            if (LocalApicAddrEntry->LocalApicAddr) {
                ApicInfo.LocalApicAddr = LocalApicAddrEntry->LocalApicAddr;
            }
        }

        MadtEntryPointer += MadtEntryFlagPointer->length;
        i                += MadtEntryFlagPointer->length;
    }
    write_serial_string("Local APIC Address Get Success.\n");
    write_serial_string("LocalApicAddress: ");
    write_serial_hex((UINT64)(ApicInfo.LocalApicAddr));
    write_serial_string("\n");

    // 启用xAPIC和x2APIC（好像这个寄存器在xAPIC模式下也能用MSR操作）
    UINT32 ApicMsrEax, ApicMsrEdx;
    GetMSR(MSR_IA32_APIC_BASE, &ApicMsrEax, &ApicMsrEdx);
    ApicMsrEax |= xAPIC_ENABLE;             	// 0000 0000 0000 0000 0000 1000 0000 0000 启用APIC
    ApicMsrEax |= x2APIC_ENABLE;        		// 0000 0000 0000 0000 0000 0100 0000 0000 启用x2APIC
    SetMSR(MSR_IA32_APIC_BASE,  ApicMsrEax,  ApicMsrEdx);
    GetMSR(MSR_IA32_APIC_BASE, &ApicMsrEax, &ApicMsrEdx);
    // 检测是否启用
    if (ApicMsrEax & 0xC00) { 
        write_serial_string("x2APIC Enabled Success.\n");
	} else if (ApicMsrEax & 0x0800) {
		write_serial_string("xAPIC Enabled Success.\n");
		ApicInfo.x2Apic = false;
	} else {
		wsod_apic_not_found();
	}

    read_lapic_reg(LAPIC_VERSION_OFFEST, &ApicMsrEax, &ApicMsrEdx);
    UINT32 local_apic_max_LVT_entries   = ((ApicMsrEax >> 16) & 0xff) + 1;
    UINT32 local_apic_version           = ApicMsrEax & 0xff;
    write_serial_string("Local APIC Version: ");            // APIC版本
    write_serial_hex(local_apic_version);
    write_serial_string("\n");
    write_serial_string("Max LVT Entry: ");                 // 最大LVT入口，根据英特尔手册看需要+1，奔4和志强有6个，P6系列有5个，Nehalem架构有7个
    write_serial_dec(local_apic_max_LVT_entries);
    write_serial_string("\n");
    write_serial_string("SVR(Suppress EOI Broadcast): ");   // 是否支持抑制EOI广播
    write_serial_dec((ApicMsrEax >> 24) & 0x1);
    write_serial_string("\n");
    // 检测是封装在CPU里还是独立的分离的
    if ((ApicMsrEax & 0xff) < 0x10)
        write_serial_string("82489DX discrete APIC\n");
    else if (((ApicMsrEax & 0xff) >= 0x10) && ((ApicMsrEax & 0xff) <= 0x15))
        write_serial_string("Integrated APIC.\n");

    // LVT
	write_lapic_reg(LAPIC_LVT_TIMER_OFFEST, 32, 0);
    
    // 设置SVR寄存器，启用Local APIC
    read_lapic_reg(LAPIC_SVR_OFFEST, &ApicMsrEax, &ApicMsrEdx);
    ApicMsrEax |= IA32_APIC_SVR_MSR_LAPIC;      // 0000 0000 0000 0000 0000 0001 0000 0000 启用LocalAPIC
    write_lapic_reg(LAPIC_SVR_OFFEST, ApicMsrEax,  ApicMsrEdx);
    read_lapic_reg(LAPIC_SVR_OFFEST, &ApicMsrEax, &ApicMsrEdx);
    // 检测是否成功设置
    if (ApicMsrEax & 0x100) 
        write_serial_string("Local APIC Enabled Success.\n");

    write_lapic_reg(LAPIC_SVR_OFFEST, 0b1011, 0);

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

    // 屏蔽8259A
    disable_8259A();
    write_serial_string("Mask 8259A Success.\n");
    io_mfence();

    switch (check_apic()) 
    {
    case 2:
        write_serial_string("You PC support x2APIC\n");
        ApicInfo.x2Apic = true;
        init_lApic(MADT);
        break;
    case 1:
        write_serial_string("You PC support xAPIC\n");
        ApicInfo.x2Apic = false;
        init_lApic(MADT);
        break;
    default:
        wsod_apic_not_found();
        break;          // 虽然这里理论上永远都不会执行...
    }

    init_IOApic(MADT);

    write_serial_string("APIC Initialize Success.\n");
}

void ApicEoi() {
	write_lapic_reg(LAPIC_EOI_OFFEST, 0, 0);
}
