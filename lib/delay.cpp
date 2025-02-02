#include "../include/include.h"

// CPUID指令包装函数
static inline void cpuid(uint32_t leaf, uint32_t subleaf,
                         uint32_t *eax, uint32_t *ebx, uint32_t *ecx, uint32_t *edx)
{
    __asm__ volatile("cpuid"
                     : "=a"(*eax), "=b"(*ebx), "=c"(*ecx), "=d"(*edx)
                     : "a"(leaf), "c"(subleaf));
}

// 读取MSR寄存器
static inline int rdmsr(uint32_t msr, uint64_t *value)
{
    uint32_t low, high;

    __asm__ volatile("rdmsr"
                     : "=a"(low), "=d"(high)
                     : "c"(msr));

    *value = ((uint64_t)high << 32) | low;
    return 0;
}

// 写入MSR寄存器
static inline int wrmsr(uint32_t msr, uint64_t value)
{
    uint32_t low = value & 0xFFFFFFFF;
    uint32_t high = value >> 32;

    __asm__ volatile("wrmsr"
                     :
                     : "c"(msr), "a"(low), "d"(high));
    return 0;
}

// 获取CPU基础频率（MHz）
uint64_t get_cpu_base_freq()
{
    uint32_t eax, ebx, ecx, edx;

    // 检查是否支持扩展功能
    cpuid(0x80000000, 0, &eax, &ebx, &ecx, &edx);
    if (eax < 0x80000004)
    {
        return 2000; // 如果不支持，返回默认值
    }

    // 获取处理器名称字符串
    char brand[49];
    xmemset(brand, 0, sizeof(brand));

    for (uint32_t i = 0; i < 3; i++)
    {
        cpuid(0x80000002 + i, 0,
              (uint32_t *)(brand + i * 16),
              (uint32_t *)(brand + i * 16 + 4),
              (uint32_t *)(brand + i * 16 + 8),
              (uint32_t *)(brand + i * 16 + 12));
    }

    // 从处理器名称中提取频率
    char *ghz_pos = strstr(brand, "GHz");
    if (ghz_pos)
    {
        // 向前搜索数字
        char *num_start = ghz_pos;
        while (num_start > brand && (*num_start < '0' || *num_start > '9'))
        {
            num_start--;
        }
        while (num_start > brand && ((*num_start >= '0' && *num_start <= '9') || *num_start == '.'))
        {
            num_start--;
        }
        num_start++;

        // 转换为MHz
        float ghz = 0.0f;
        if (str2float(num_start, &ghz) == 1)
        {
            return (uint64_t)(ghz * 1000); // 转换为MHz
        }
    }

    // 尝试使用性能监控叶子
    cpuid(0x16, 0, &eax, &ebx, &ecx, &edx);
    if (eax != 0)
    {
        return eax; // 返回基础频率（MHz）
    }

    // 如果上述方法都失败，使用MSR
    uint64_t platform_info;
    if (rdmsr(0x15, &platform_info) == 0)
    {
        uint64_t base_freq = (platform_info >> 8) & 0xFF;
        return base_freq * 100; // 转换为MHz
    }

    return 2000; // 如果所有方法都失败，返回默认值
}

// 获取当前CPU频率（MHz）
uint64_t get_cpu_freq_mhz()
{
    static uint64_t cached_freq = 0;
    if (cached_freq > 0)
    {
        return cached_freq;
    }

    uint32_t eax, ebx, ecx, edx;

    // 首先尝试使用扩展性能监控叶子
    cpuid(0x15, 0, &eax, &ebx, &ecx, &edx);
    if (eax != 0 && ebx != 0)
    {
        uint64_t crystal_freq = ecx; // 晶振频率
        uint64_t core_freq = (crystal_freq * ebx) / eax;
        if (core_freq > 0)
        {
            cached_freq = core_freq;
            return cached_freq;
        }
    }

    // 如果上述方法失败，使用基础频率
    cached_freq = get_cpu_base_freq();
    return cached_freq;
}

// 读取TSC计数器
static inline uint64_t rdtsc()
{
    uint32_t low, high;
    __asm__ volatile("rdtsc" : "=a"(low), "=d"(high));
    return ((uint64_t)high << 32) | low;
}

// 微秒级延时
void delay_us(uint64_t us)
{
    static uint64_t freq_mhz = get_cpu_freq_mhz();
    uint64_t ticks = us * freq_mhz; // 需要等待的CPU周期数
    uint64_t start = rdtsc();
    uint64_t end = start + ticks;

    // 处理计数器溢出的情况
    if (end < start)
    {
        while (rdtsc() >= start)
        {
            __asm__ volatile("nop");
        }
    }

    while (rdtsc() < end)
    {
        __asm__ volatile("nop");
    }
}

// 毫秒级延时
void delay_ms(uint64_t ms)
{
    delay_us(ms * 1000);
}

// 秒级延时
void delay_s(uint64_t s)
{
    for (uint64_t i = 0; i < s; i++)
    {
        delay_ms(1000);
    }
}
