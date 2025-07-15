#pragma once

// 我操你妈C++，不给_Atomic是什么意思？？？是因为把你妈骨灰盒打包进C++标准里了所以没地方了？
// 不兼容C还敢叫C++，操你妈
// FUCK YOU C++, YOU SON OF BITCH, WHERE IS MY _Atomic ??? YOU MOTHER FUCKER
#ifdef __cplusplus
#define CPP_IS_A_SON_OF_BITCH
typedef volatile bool spin_t;
#else
#define bool _Bool
#define true 1
#define false 0
typedef _Atomic volatile bool spin_t;
#endif

#define SPIN_INIT false

#define spin_trylock(_spin_) (!__atomic_test_and_set(&(_spin_), 5))
#define spin_lock(_spin_)                                                                          \
    ({                                                                                             \
        while (__atomic_test_and_set(&(_spin_), 5)) {}                                             \
    })
#define spin_unlock(_spin_) __atomic_clear(&(_spin_), 5)

#define spin_lockall(...)                                                                          \
    ({                                                                                             \
        const spin_t _spins_[] = {__VA_ARGS__};                                                    \
        loop {                                                                                     \
            usize _i_ = 0;                                                                         \
            for (; _i_ < COUNT(_spins_); _i_++) {                                                  \
                if (!spin_trylock(_spins_[_i_])) break;                                            \
            }                                                                                      \
            if (_i_ == COUNT(_spins_)) break;                                                      \
            for (; _i_ > 0; _i_--) {                                                               \
                spin_unlock(_spins_[_i_ - 1]);                                                     \
            }                                                                                      \
        }                                                                                          \
    })

#define spin_unlockall(...)                                                                        \
    ({                                                                                             \
        const spin_t _spins_[] = {__VA_ARGS__};                                                    \
        for (usize _i_ = COUNT(_spins_); _i_ > 0; _i_--) {                                         \
            spin_unlock(_spins_[_i_ - 1]);                                                         \
        }                                                                                          \
    })

#define barrier() __asm__ volatile("" : : : "memory")

#define cpu_relax()                                                                                \
    do {                                                                                           \
        __asm__ volatile("pause\n" : : : "memory");                                                \
    } while (false);
