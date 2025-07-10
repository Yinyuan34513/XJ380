#ifndef MUTEX_H
#define MUTEX_H

#include "apic/apic.h"

// 自旋锁结构定义
typedef struct {
    volatile uint32_t lock;  // 锁状态，0表示未锁定，1表示已锁定
} SpinLock;

// 互斥锁结构定义
typedef struct {
    SpinLock spinLock;       // 内部自旋锁
    int owner;               // 持有锁的CPU核心ID
    int recursionCount;      // 递归加锁计数
} Mutex;

// 自旋锁操作函数声明
void initializeSpinLock(SpinLock* lock);
void acquireSpinLock(SpinLock* lock);
void releaseSpinLock(SpinLock* lock);
bool tryAcquireSpinLock(SpinLock* lock);

// 互斥锁操作函数声明
void initializeMutex(Mutex* mutex);
void lockMutex(Mutex* mutex);
void unlockMutex(Mutex* mutex);
bool tryLockMutex(Mutex* mutex);

#endif // MUTEX_H