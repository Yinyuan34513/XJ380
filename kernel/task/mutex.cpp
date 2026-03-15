#include "mutex.h"
#include "task/scheduler.h"
#include "cpu/lock.h"
#include "mm/heap.h"
#include "krlibc.h"
#include <proto.hpp>

// 当前系统已创建的互斥锁队列
// static lock_queue *global_mutex_queue = NULL;
// static spin_t global_mutex_lock = SPIN_INIT;
static size_t next_mutex_id = 1;

// 初始化互斥锁子系统
// void mutex_init_system() {
//     if (global_mutex_queue == NULL) {
//         global_mutex_queue = queue_init();
//     }
// }

// 创建互斥锁
void mutex_create(mutex_t* mtx,bool recursive) {
    // mutex_init_system();//en. 懒得放了
    
    mtx->lock = SPIN_INIT;
    mtx->state = MUTEX_UNLOCKED;
    mtx->owner = NULL;
    mtx->wait_queue = queue_init();
    mtx->rcc = 0;
    mtx->rec = recursive;
    
    // 添加到全局队列
    // spin_lock(&global_mutex_lock);
    // queue_enqueue_id(global_mutex_queue, mutex, next_mutex_id++);
    // spin_unlock(&global_mutex_lock);
}

// 获取互斥锁（阻塞）
int mutex_lock(mutex_t *mutex) {
    if (mutex == NULL || mutex->state == MUTEX_DESTROYED) {
        return -EINVAL;
    }
    
    tcb_t current = get_current_task();
    
    spin_lock(&mutex->lock);
    
    // 如果是递归锁且当前线程已经持有锁
    if (mutex->rec && mutex->owner == current) {
        mutex->rcc++;
        spin_unlock(&mutex->lock);
        return 0;
    }
    
    // 如果锁可用，立即获取
    if (mutex->state == MUTEX_UNLOCKED) {
        mutex->state = MUTEX_LOCKED;
        mutex->owner = current;
        mutex->rcc = 1;
        spin_unlock(&mutex->lock);
        return 0;
    }
    
    // 锁被占用，加入等待队列并阻塞
    queue_enqueue(mutex->wait_queue, current);
    current->status = WAIT;  // 阻塞线程,我记得
    
    spin_unlock(&mutex->lock);
    
    // 让出CPU
    scheduler_yield();
    
    return 0;
}

// 尝试获取互斥锁（非阻塞）
int mutex_trylock(mutex_t *mutex) {
    if (mutex == NULL || mutex->state == MUTEX_DESTROYED) {
        return -EINVAL;
    }//锁没了
    
    tcb_t current = get_current_task();//如果task动了? ..应该不可能
    
    spin_lock(&mutex->lock);
    
    // 如果是递归锁且当前线程已经持有锁
    if (mutex->rec && mutex->owner == current) {
        mutex->rcc++;
        spin_unlock(&mutex->lock);
        return 0;
    }
    
    // 检查锁是否可用
    if (mutex->state == MUTEX_UNLOCKED) {
        mutex->state = MUTEX_LOCKED;
        mutex->owner = current;
        mutex->rcc = 1;
        spin_unlock(&mutex->lock);
        return 0;
    }
    
    // 锁被占用，立即返回失败
    spin_unlock(&mutex->lock);
    return -EBUSY;
}

// 释放互斥锁
int mutex_unlock(mutex_t *mutex) {
    if (mutex == NULL || mutex->state == MUTEX_DESTROYED) {
        return -EINVAL;
    }
    
    tcb_t current = get_current_task();
    
    spin_lock(&mutex->lock);
    
    // 检查调用者是否持有锁
    if (mutex->owner != current) { //当前进程不是所有者, 按理说只有所有者可以解锁
        spin_unlock(&mutex->lock);
        return -EPERM;
    }
    
    // 减少递归计数
    mutex->rcc--;
    
    // 如果递归计数为0，释放锁
    if (mutex->rcc == 0) {
        // 唤醒等待队列中的第一个线程
        tcb_t next_thread = (tcb_t)queue_dequeue(mutex->wait_queue);
        if (next_thread != NULL) {
            mutex->owner = next_thread;
            mutex->rcc = 1;
            next_thread->status = START;  // 设置为可运行
            // 注意：任务应该已经在调度队列中，不需要重复调用add_task
        } else {
            mutex->state = MUTEX_UNLOCKED;
            mutex->owner = NULL;
        }
    }
    
    spin_unlock(&mutex->lock);
    return 0;
}

// 销毁互斥锁
int mutex_destroy(mutex_t *mutex) {
    if (mutex == NULL) {
        return -1;
    }
    
    spin_lock(&mutex->lock);
    if(mutex->wait_queue->size >0)
    {
        spin_unlock(&mutex->lock);
        return -EOPNOTSUPP;
    }
    
    // 标记为已销毁
    mutex->state = MUTEX_DESTROYED;
    
    // 保存等待队列指针以便释放
    lock_queue *wait_queue = mutex->wait_queue;
    mutex->wait_queue = NULL;
    
    spin_unlock(&mutex->lock);
    
    // 释放资源
    if (wait_queue != NULL) {
        queue_destroy(wait_queue);
    }
    free(mutex);
    return 0;
}

// 检查锁是否被锁定
bool mutex_is_locked(mutex_t *mutex) {
    if (mutex == NULL) {
        return false;
    }
    
    spin_lock(&mutex->lock);
    bool locked = (mutex->state == MUTEX_LOCKED);
    spin_unlock(&mutex->lock);
    
    return locked;
}

// 获取锁的当前持有者
tcb_t mutex_get_owner(mutex_t *mutex) {
    if (mutex == NULL) {
        return NULL;
    }
    
    spin_lock(&mutex->lock);
    tcb_t owner = mutex->owner;
    spin_unlock(&mutex->lock);
    
    return owner;
}