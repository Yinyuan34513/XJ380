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
