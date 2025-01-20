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
