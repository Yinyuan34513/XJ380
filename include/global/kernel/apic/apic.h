#ifndef _APIC_H_
#define _APIC_H_

#pragma pack(1)
#ifdef __cplusplus
extern "C" {
#endif
#include "../../../efi.h"
#ifdef __cplusplus
}
#endif

#include "../../stdint.h"

// CPUID命令
#define CPUID_ECX_x2APIC        1 << 21
#define CPUID_EDX_APIC          1 << 9

#pragma pack()

#endif