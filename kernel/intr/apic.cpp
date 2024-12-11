#include "../../include/include.h"

/** 
 * 
 * @brief CPUID
 * 
 * @param mop 格式（0=CPU厂商 1=CPU参数）
 * 
 */
void asm_cpuid(UINT32 mop, UINT32 sop, UINT32 *eax, UINT32 *ebx, UINT32 *ecx, UINT32 *edx) {
    __asm__ volatile ("cpuid \n\t" : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx) : "0"(mop), "2"(sop) : "memory");
}

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
        return 1;
    }
    return 0;
}

/** 
 * 
 * @brief 初始化Local APIC
 * 
 * @param MADT MADT表指针
 * 
 */
void init_LocalApic(UINT64 *MADT) {
    //
}
