#ifndef __HPET_H__
#define __HPET_H__

#pragma pack(1)
#ifdef __cplusplus
extern "C" {
#endif
#include "../../../efi.h"
#ifdef __cplusplus
}
#endif

#include "../../stdint.h"
#include "../longm.h"

struct generic_address {
    UINT8 address_space;
    UINT8 bit_width;
    UINT8 bit_offset;
    UINT8 access_size;
    UINT64 address;
} __attribute__((packed));

struct ACPISDTHeader {
    char Signature[4];
    UINT32 Length;
    UINT8 Revision;
    UINT8 Checksum;
    char OEMID[6];
    char OEMTableID[8];
    UINT32 OEMRevision;
    UINT32 CreatorID;
    UINT32 CreatorRevision;
};

struct hpet {
    struct ACPISDTHeader h;
    UINT32 event_block_id;
    struct generic_address base_address;
    UINT16 clock_tick_unit;
    UINT8 page_oem_flags;
}__attribute__((packed));

typedef struct {
    UINT64 configurationAndCapability;
    UINT64 comparatorValue;
    UINT64 fsbInterruptRoute;
    UINT64 unused;
} __attribute__((packed)) HpetTimer;

typedef struct {
    UINT64 generalCapabilities;
    UINT64 reserved0;
    UINT64 generalConfiguration;
    UINT64 reserved1;
    UINT64 generalIntrruptStatus;
    UINT8 reserved3[0xc8];
    UINT64 mainCounterValue;
    UINT64 reserved4;
    HpetTimer timers[];
} __attribute__((packed)) volatile HpetInfo;

typedef struct generic_address GenericAddress;
typedef struct hpet Hpet;

#pragma pack()

#endif
