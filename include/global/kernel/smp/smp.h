#ifndef SMP_H
#define SMP_H

#include "../../stdint.h"
#include "../apic/apic.h"

#define MAX_CPU_NUM 32

class SMP
{
public:
    SMP();
    ~SMP();
    void ap_startup(uint64_t madt_addr);
    int ap_count();

private:
    int apu_count;
    MadtLocalApic *apu_list[MAX_CPU_NUM];
};

extern SMP SMP_MANAGER;

uint64_t get_cpuid_by_lapicid(uint64_t lapicid);

#endif
