#include "../../include/include.h"

// MSR都有点年份了要是没有那真就美酒了
/**
 * @brief 获取MSR
 *
 * @param msr MSR编号
 * @param lo  低32位
 * @param hi  高32位
 */
void GetMSR(UINT32 msr, UINT32 *lo, UINT32 *hi)
{
   asm volatile("rdmsr" : "=a"(*lo), "=d"(*hi) : "c"(msr) : "memory");
}

/**
 * @brief 设置MSR
 *
 * @param msr MSR编号
 * @param lo  低32位
 * @param hi  高32位
 */
void SetMSR(UINT32 msr, UINT32 lo, UINT32 hi)
{
   asm volatile("wrmsr" : : "a"(lo), "d"(hi), "c"(msr) : "memory");
}
