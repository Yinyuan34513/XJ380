#include <cpu/fpu.h>

void save_fpu_context(fpu_context_t *ctx) {
    __asm__ volatile("fnsave %0" : "=m"(ctx->fxsave_area)::"memory");
}

void restore_fpu_context(fpu_context_t *ctx) {
    __asm__ volatile("frstor %0" : : "m"(ctx->fxsave_area) : "memory");
}
