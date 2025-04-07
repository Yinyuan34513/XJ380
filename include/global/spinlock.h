#ifndef _SPINLOCK_H
#define _SPINLOCK_H

typedef struct
{
    __volatile__ unsigned long lock; // 1:unlock,0:lock
} spinlock_t;

inline void spin_init(spinlock_t *lock)
{
    lock->lock = 1;
}

inline void spin_lock(spinlock_t *lock)
{
    __asm__ __volatile__("1:	\n\t"
                         "lock	decq	%0	\n\t"
                         "jns	3f	\n\t"
                         "2:	\n\t"
                         "pause	\n\t"
                         "cmpq	$0,	%0	\n\t"
                         "jle	2b	\n\t"
                         "jmp	1b	\n\t"
                         "3:	\n\t"
                         : "=m"(lock->lock)
                         :
                         : "memory");
}

inline void spin_unlock(spinlock_t *lock)
{
    __asm__ __volatile__("movq	$1,	%0	\n\t"
                         : "=m"(lock->lock)
                         :
                         : "memory");
}

#endif
