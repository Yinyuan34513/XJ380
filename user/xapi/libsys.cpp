#include "./include/stdint.h"

uint64_t enter_syscall(uint64_t syscall_number, uint64_t arg1, uint64_t arg2, uint64_t arg3,
                       uint64_t arg4, uint64_t arg5, uint64_t arg6) {
    uint64_t ret;
    __asm__ __volatile__("movq %2, %%rdi\n\t"
                         "movq %3, %%rsi\n\t"
                         "movq %4, %%rdx\n\t"
                         "movq %5, %%r10\n\t"
                         "movq %6, %%r8\n\t"
                         "movq %7, %%r9\n\t"
                         "syscall\n\t"
                         : "=a"(ret)
                         : "a"(syscall_number), "r"(arg1), "r"(arg2), "r"(arg3), "r"(arg4),
                           "r"(arg5), "r"(arg6)
                         : "memory", "rdi", "rsi", "rdx", "r10", "r8", "r9");
    return ret;
}
