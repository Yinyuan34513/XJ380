#ifndef _MADT_H_
#define _MADT_H_

#pragma pack(1)
#ifdef __cplusplus
extern "C"
{
#endif
#include "../../../efi.h"
#ifdef __cplusplus
}
#endif

#include "../../stdint.h"

typedef struct
{
    UINT32 LocalApicAddr; // Local APIC 地址
    UINT32 PicSign;       // 1=安装了8259A
} __attribute__((packed)) MADT_FLAG;

typedef struct
{
    UINT8 EntryType;
    UINT8 length;
} __attribute__((packed)) MADT_ENTRY_FLAG;

// EntryType = 0
typedef struct
{
    UINT8 AcpiProcessorID;
    UINT8 ApicID;
    UINT32 Flags; // bit 0 = Processor Enabled, bit 1 = Online Capable 我也不知道啥意思
} __attribute__((packed)) MADT_LOCAL_APIC_FLAG;

// EntryType = 1
typedef struct
{
    UINT8 IOApicID;
    UINT8 reserved;
    UINT32 IOApicAddr;
    UINT32 GlobalSysIntrBase;
} __attribute__((packed)) MADT_IO_APIC_FLAG;

// EntryType = 2
typedef struct
{
    UINT8 BusScr;
    UINT8 IrqScr;
    UINT32 GlobalSysIntr;
    UINT16 Flags;
} __attribute__((packed)) MADT_IO_APIC_InterScrOverride;

// EntryType = 3
typedef struct
{
    UINT8 NMIScr;
    UINT8 reserved;
    UINT16 Flags;
    UINT32 GlobalSysIntr;
} __attribute__((packed)) MADT_IO_APIC_NMIScr;

// EntryType = 4
typedef struct
{
    UINT8 AcpiProcessorID;
    UINT16 Flags;
    UINT8 LINT; // LINT#（0或1）
} __attribute__((packed)) MADT_LOCAL_APIC_NMI;

// EntryType = 5
typedef struct
{
    UINT16 reserved;
    UINT64 LocalApicAddr;
} __attribute__((packed)) MADT_LOCAL_APIC_AddrOverride;

// EntryType = 9
typedef struct
{
    UINT16 reserved;
    UINT32 LocalApicID;
    UINT32 Flags; // bit 0 = Processor Enabled, bit 1 = Online Capable 我也不知道啥意思
    UINT32 AcpiFlags;
} __attribute__((packed)) MADT_LOCAL_x2APIC;

#pragma pack()

#endif