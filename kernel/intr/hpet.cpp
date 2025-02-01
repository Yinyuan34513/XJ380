#include "../../include/include.h"

HpetInfo *info;
static UINT32 hpetPeriod = 0;

UINT64 nanoTime()
{
    if (info == (void *)0)
        return 0;
    UINT64 mcv = info->mainCounterValue;
    return mcv * hpetPeriod;
}

void init_hpet(UINT64 hpet_ptr)
{
    info = (HpetInfo *)((Hpet *)hpet_ptr)->base_address.address;
    UINT32 counterClockPeriod = info->generalCapabilities >> 32;
    hpetPeriod = counterClockPeriod / 1000000;
    info->generalConfiguration |= 1;
    write_serial_string("Setup acpi hpet table (nano_time: ");
    write_serial_dec((uint64_t)nanoTime());
    write_serial_string(").\n");
}
