#include "../../include/include.h"

struct generic_address {
    UINT8 address_space;
    UINT8 bit_width;
    UINT8 bit_offset;
    UINT8 access_size;
    UINT64 address;
}__attribute__((packed));

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

HpetInfo *info;
static UINT32 hpetPeriod = 0;

UINT64 nanoTime() {
    if (info == (void*)0) return 0;
    UINT64 mcv = info->mainCounterValue;
    return mcv * hpetPeriod;
}

void hpet_init(UINT64 hpet_ptr){
    info = (HpetInfo*)((Hpet*)hpet_ptr)->base_address.address;
    UINT32 counterClockPeriod = info->generalCapabilities >> 32;
    hpetPeriod = counterClockPeriod / 1000000;
    info->generalConfiguration |= 1;
    write_serial_string("Setup acpi hpet table (nano_time: ");
    write_serial_dec((uint64_t)nanoTime());
    write_serial_string(").\n");
}
