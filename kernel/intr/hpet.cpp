#include <stdint.h>
#include <proto.h>
#include <apic/hpet.h>

HpetInfo *info;
static uint32_t hpetPeriod = 0;

uint64_t nanoTime()
{
    if (info == (void *)0)
        return 0;
    uint64_t mcv = info->mainCounterValue;
    return mcv * hpetPeriod;
}

void init_hpet(uint64_t hpet_ptr)
{
    info = (HpetInfo *)(((Hpet *)hpet_ptr)->base_address.address + 0xFFFF800000000000);
    uint32_t counterClockPeriod = info->generalCapabilities >> 32;
    hpetPeriod = counterClockPeriod / 1000000;
    info->generalConfiguration |= 1;
    write_serial_string("Setup acpi hpet table (nano_time: ");
    write_serial_dec((uint64_t)nanoTime());
    write_serial_string(").\n");
}