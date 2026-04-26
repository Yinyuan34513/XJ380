#pragma once

#include <stdint.h>
#include <task/scheduler.h>
#include <krlibc.h>

typedef struct spinlock {
    volatile long lock;
    long rflags;
} spin_t;

#define SPIN_INIT (spin_t){0, 0}


static inline void spin_init(spin_t *lock) {
    memset(lock, 0, sizeof(spin_t));
}

static inline void spin_lock(spin_t *lock) {
    long flags;
    asm volatile("pushfq\n\t"
                 "pop %0\n\t"
                 : "=r"(flags)
                 :
                 : "memory");

    asm volatile("cli\n\t" ::: "memory");

    asm volatile("1:\n\t"
                 "lock btsq $0, %0\n\t" // 测试并设置
                 "jnc 2f\n\t"
                 "pause\n\t"
                 "jmp 1b\n\t"
                 "2:\n\t"
                 : "+m"(lock->lock)
                 :
                 : "memory", "cc");

    asm volatile("mfence" ::: "memory");

    lock->rflags = flags; // 保存原始中断状态
}

static inline void spin_lock_no_irqsave(spin_t *lock) {
    asm volatile("1:\n\t"
                 "lock btsq $0, %0\n\t" // 测试并设置
                 "jnc 2f\n\t"
                 "pause\n\t"
                 "jmp 1b\n\t"
                 "2:\n\t"
                 : "+m"(lock->lock)
                 :
                 : "memory", "cc");

    asm volatile("mfence" ::: "memory");
}

static inline void spin_unlock(spin_t *lock) {
    asm volatile("lock btrq $0, %0\n\t" // 清除锁标志
                 : "+m"(lock->lock)
                 :
                 : "memory", "cc");

    long flags = lock->rflags;
    asm volatile("push %0\n\t" // 恢复原始RFLAGS
                 "popfq"
                 :
                 : "r"(flags)
                 : "memory");

    asm volatile("sfence" ::: "memory");
}

static inline void spin_unlock_no_irqstore(spin_t *lock) {
    asm volatile("lock btrq $0, %0\n\t" // 清除锁标志
                 : "+m"(lock->lock)
                 :
                 : "memory", "cc");

    asm volatile("sfence" ::: "memory");
}


#define barrier() __asm__ volatile("" : : : "memory")

#define cpu_relax()                                                                                                    \
    do                                                                                                                 \
    {                                                                                                                  \
        __asm__ volatile("pause\n" : : : "memory");                                                                    \
    } while (false);
