#include <stdint.h>

static inline uint64_t rdtsc() {
    uint32_t edx, eax;
    asm volatile ("rdtsc" : "=a" (eax), "=d" (edx));
    return ((uint64_t)edx << 32) | eax;
}

static inline void delay(uint64_t cycles) {
    uint64_t next_stop = rdtsc() + cycles;

    while (rdtsc() < next_stop);
}