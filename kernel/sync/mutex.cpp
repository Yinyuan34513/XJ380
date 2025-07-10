#include "mutex.h"
#include "proto.h"
static UINT32 getCpuId() {
    return lapic_read(LAPIC_REG_ID) >> 24;
}
// 自旋锁初始化
void initializeSpinLock(SpinLock* lock) {
    lock->lock = 0;
}

// 获取自旋锁
void acquireSpinLock(SpinLock* lock) {
    while (__atomic_test_and_set(&lock->lock, __ATOMIC_ACQUIRE)) {
        asm volatile("pause");
    }
}

// 释放自旋锁
void releaseSpinLock(SpinLock* lock) {
    __atomic_clear(&lock->lock, __ATOMIC_RELEASE);
}

// 尝试获取自旋锁
bool tryAcquireSpinLock(SpinLock* lock) {
    return !__atomic_test_and_set(&lock->lock, __ATOMIC_ACQUIRE);
}

// 互斥锁初始化
void initializeMutex(Mutex* mutex) {
    initializeSpinLock(&mutex->spinLock);
    mutex->owner = -1;
    mutex->recursionCount = 0;
}

// 获取互斥锁
void lockMutex(Mutex* mutex) {
    int cpuId = getCpuId();
    
    acquireSpinLock(&mutex->spinLock);
    if (mutex->owner == cpuId) {
        // 当前CPU已经持有锁，递归加锁
        mutex->recursionCount++;
        releaseSpinLock(&mutex->spinLock);
        return;
    }
    
    // 等待直到锁被释放
    while (mutex->owner != -1) {
        releaseSpinLock(&mutex->spinLock);
        asm volatile("pause");
        acquireSpinLock(&mutex->spinLock);
    }
    
    // 获取锁
    mutex->owner = cpuId;
    mutex->recursionCount = 1;
    releaseSpinLock(&mutex->spinLock);
}

// 释放互斥锁
void unlockMutex(Mutex* mutex) {
    int cpuId = getCpuId();
    
    acquireSpinLock(&mutex->spinLock);
    if (mutex->owner != cpuId) {
        // 不是锁的持有者，释放自旋锁并返回
        releaseSpinLock(&mutex->spinLock);
        return;
    }
    
    // 减少递归计数
    mutex->recursionCount--;
    if (mutex->recursionCount == 0) {
        mutex->owner = -1;
    }
    releaseSpinLock(&mutex->spinLock);
}

// 尝试获取互斥锁
bool tryLockMutex(Mutex* mutex) {
    int cpuId = getCpuId();
    
    if (!tryAcquireSpinLock(&mutex->spinLock)) {
        return false;
    }
    
    if (mutex->owner == cpuId) {
        // 递归加锁
        mutex->recursionCount++;
        releaseSpinLock(&mutex->spinLock);
        return true;
    }
    
    if (mutex->owner == -1) {
        // 获取锁
        mutex->owner = cpuId;
        mutex->recursionCount = 1;
        releaseSpinLock(&mutex->spinLock);
        return true;
    }
    
    // 锁已被其他CPU持有
    releaseSpinLock(&mutex->spinLock);
    return false;
}