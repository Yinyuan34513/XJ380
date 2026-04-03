extern "C"{
#include "../../../include/rtc.h"
#include "../../../include/task/pcb.h"
#include "../../../include/task/scheduler.h"
#include "lwip/arch.h"
#include "sys_arch.h"
#include "lwipopts.h"
#include <lwip/arch.h>
#include <lwip/debug.h>
#include <lwip/opt.h>
#include <lwip/stats.h>
#include <lwip/sys.h>
#include "../../../include/proto.hpp"
#include "utils.h"
}
int errno_ = 0;

// lwip glue code for cavOS
// Copyright (C) 2024 Panagiotis
extern "C" {

static volatile long g_sys_arch_protect_lock = 0;
static void         *g_sys_arch_protect_owner = NULL;
static u32_t         g_sys_arch_protect_depth = 0;
static sys_prot_t    g_sys_arch_protect_flags = 0;

static inline void *sys_arch_current_owner(void)
{
    return (void *)get_current_task();
}

static inline sys_prot_t sys_arch_read_rflags(void)
{
    sys_prot_t flags = 0;
    asm volatile("pushfq\n\t"
                 "pop %0"
                 : "=r"(flags)
                 :
                 : "memory");
    return flags;
}

void sys_init(void) {
    // boo!
}

sys_prot_t sys_arch_protect(void) {
    void *owner = sys_arch_current_owner();
    if (g_sys_arch_protect_owner == owner && g_sys_arch_protect_depth > 0) {
        g_sys_arch_protect_depth++;
        return g_sys_arch_protect_flags;
    }

    sys_prot_t flags = sys_arch_read_rflags();
    asm volatile("cli" ::: "memory");

    while (!__sync_bool_compare_and_swap(&g_sys_arch_protect_lock, 0, 1)) {
        asm volatile("pause" ::: "memory");
    }

    g_sys_arch_protect_owner = owner;
    g_sys_arch_protect_depth = 1;
    g_sys_arch_protect_flags = flags;
    return flags;
}

void sys_arch_unprotect(sys_prot_t pval) {
    (void)pval;

    void *owner = sys_arch_current_owner();
    if (g_sys_arch_protect_owner != owner || g_sys_arch_protect_depth == 0) {
        return;
    }

    g_sys_arch_protect_depth--;
    if (g_sys_arch_protect_depth != 0) {
        return;
    }

    sys_prot_t flags = g_sys_arch_protect_flags;
    g_sys_arch_protect_owner = NULL;
    g_sys_arch_protect_flags = 0;
    __sync_lock_release(&g_sys_arch_protect_lock);

    asm volatile("push %0\n\t"
                 "popfq"
                 :
                 : "r"(flags)
                 : "memory", "cc");
}

void sys_mutex_lock(sys_mutex_t *mutex) {
    if (mutex == NULL) {
        return;
    }

    void *current = (void *)get_current_task();
    for (;;) {
        if (!mutex->valid) {
            return;
        }
        if (mutex->owner == current) {
            mutex->depth++;
            return;
        }
        if (__sync_bool_compare_and_swap(&mutex->state, 0, 1)) {
            mutex->owner = current;
            mutex->depth = 1;
            return;
        }
        scheduler_yield();
        asm volatile("pause" ::: "memory");
    }
}
void sys_mutex_unlock(sys_mutex_t *mutex) {
    if (mutex == NULL) {
        return;
    }

    void *current = (void *)get_current_task();
    if (!mutex->valid || mutex->owner != current || mutex->depth == 0) {
        return;
    }

    mutex->depth--;
    if (mutex->depth == 0) {
        mutex->owner = NULL;
        __sync_lock_release(&mutex->state);
    }
}
void sys_mutex_free(sys_mutex_t *mutex) {
    if (mutex == NULL) {
        return;
    }
    mutex->valid = false;
    mutex->owner = NULL;
    mutex->depth = 0;
    __sync_lock_release(&mutex->state);
}
int sys_mutex_valid(sys_mutex_t *mutex) {
    return mutex != NULL && mutex->valid;
}
void sys_mutex_set_invalid(sys_mutex_t *mutex) {
    if (mutex == NULL) {
        return;
    }
    mutex->valid = false;
    mutex->owner = NULL;
    mutex->depth = 0;
    __sync_lock_release(&mutex->state);
}

err_t sys_mutex_new(sys_mutex_t *mutex) {
    if (mutex == NULL) {
        return ERR_ARG;
    }
    mutex->state = 0;
    mutex->owner = NULL;
    mutex->depth = 0;
    mutex->valid = true;
    return ERR_OK;
}

err_t sys_sem_new(sys_sem_t *sem, uint8_t cnt) {
    sem->invalid = false;
    sem->cnt = cnt;
    sem->lock = SPIN_INIT;

    return ERR_OK;
}

void sys_sem_signal(sys_sem_t *sem) {
    spin_lock(&sem->lock);
    sem->cnt++;
    spin_unlock(&sem->lock);
}

uint32_t sys_arch_sem_wait(sys_sem_t *sem, uint32_t timeout) {
    uint32_t start = sys_now();

    while (true) {
        spin_lock(&sem->lock);
        if (sem->cnt > 0) {
            sem->cnt--;
            spin_unlock(&sem->lock);
            return sys_now() - start;
        }
        spin_unlock(&sem->lock);

        if (timeout != 0 && (uint32_t)(sys_now() - start) >= timeout) {
            return SYS_ARCH_TIMEOUT;
        }
        scheduler_yield();
    }
}

void sys_sem_free(sys_sem_t *sem) {
    sem->invalid = true;
    sem->cnt = 0;
}

void sys_sem_set_invalid(sys_sem_t *sem) { sem->invalid = true; }

int sys_sem_valid(sys_sem_t *sem) { return !sem->invalid; }

uint32_t sys_now(void) {
    return (u32_t)(nanoTime() / 1000000ULL);
}

sys_thread_t sys_thread_new(const char *pcName,
                            void (*pxThread)(void *pvParameters), void *pvArg,
                            int iStackSize, int iPriority) {
    
    return create_kernel_thread((void *)pxThread, pvArg, (char *)pcName, NULL);
}

err_t sys_mbox_new(sys_mbox_t *mbox, int size) {
    if (mbox == NULL) {
        return ERR_ARG;
    }
    if (!size) {
        size = TCPIP_MBOX_SIZE;
        // debugf("Tried to create a mailbox with a size of 0!\n");
        // panic();
    }
    memset(mbox, 0, sizeof(sys_mbox_t));
    mbox->invalid = false;
    mbox->lock = SPIN_INIT;
    mbox->size = size + 1; // keep one slot empty to disambiguate full/empty
    mbox->msges = (void**)calloc((size_t)mbox->size, sizeof(mbox->msges[0]));
    return mbox->msges != NULL ? ERR_OK : ERR_MEM;
}

void sys_mbox_free(sys_mbox_t *mbox) {
    if (mbox == NULL) {
        return;
    }
    spin_lock(&mbox->lock);
    mbox->invalid = true;
    free(mbox->msges);
    mbox->msges = NULL;
    mbox->ptrRead = 0;
    mbox->ptrWrite = 0;
    spin_unlock(&mbox->lock);
}

void sys_mbox_set_invalid(sys_mbox_t *mbox) {
    if (mbox == NULL) {
        return;
    }
    spin_lock(&mbox->lock);
    mbox->invalid = true;
    spin_unlock(&mbox->lock);
}

int sys_mbox_valid(sys_mbox_t *mbox) {
    if (mbox == NULL) {
        return 0;
    }
    spin_lock(&mbox->lock);
    int ret = !mbox->invalid && mbox->msges != NULL;
    spin_unlock(&mbox->lock);
    return ret;
}

void sys_mbox_post_unsafe(sys_mbox_t *q, void *msg) {
    q->msges[q->ptrWrite] = msg;
    q->ptrWrite = (q->ptrWrite + 1) % q->size;
}

void sys_mbox_post(sys_mbox_t *q, void *msg) {
    if (q == NULL) {
        return;
    }
    while (true) {
        spin_lock(&q->lock);
        if (q->invalid || q->msges == NULL) {
            spin_unlock(&q->lock);
            return;
        }
        if ((q->ptrWrite + 1) % q->size != q->ptrRead)
            break;
        spin_unlock(&q->lock);

        scheduler_yield();
    }

    sys_mbox_post_unsafe(q, msg);
    spin_unlock(&q->lock); // sys_mbox_post_unsafe不再解锁
}

err_t sys_mbox_trypost(sys_mbox_t *q, void *msg) {
    if (q == NULL) {
        return ERR_ARG;
    }
    spin_lock(&q->lock);
    if (q->invalid || q->msges == NULL) {
        spin_unlock(&q->lock);
        return ERR_ARG;
    }
    if ((q->ptrWrite + 1) % q->size == q->ptrRead) {
        spin_unlock(&q->lock);
        return ERR_MEM;
    }

    sys_mbox_post_unsafe(q, msg);
    spin_unlock(&q->lock); // sys_mbox_post_unsafe不再解锁
    return ERR_OK;
}

err_t sys_mbox_trypost_fromisr(sys_mbox_t *q, void *msg) {
    return sys_mbox_trypost(q, msg); // xd
}

u32_t sys_arch_mbox_fetch(sys_mbox_t *q, void **msg, u32_t timeout) {
    if (q == NULL) {
        return SYS_ARCH_TIMEOUT;
    }

    u32_t start = sys_now();

    while (true) {
        spin_lock(&q->lock);
        if (q->ptrRead != q->ptrWrite) {
            break;
        }
        if (q->invalid || q->msges == NULL) {
            spin_unlock(&q->lock);
            if (msg != NULL) {
                *msg = NULL;
            }
            return SYS_ARCH_TIMEOUT;
        }
        spin_unlock(&q->lock);

        if (timeout != 0 && (u32_t)(sys_now() - start) >= timeout) {
            return SYS_ARCH_TIMEOUT;
        }
        scheduler_yield();
    }

    if (msg != NULL) {
        *msg = q->msges[q->ptrRead];
    }
    q->ptrRead = (q->ptrRead + 1) % q->size;
    spin_unlock(&q->lock);

    return sys_now() - start;
}

u32_t sys_arch_mbox_tryfetch(sys_mbox_t *q, void **msg) {
    if (q == NULL) {
        return SYS_MBOX_EMPTY;
    }
    spin_lock(&q->lock);
    if (q->invalid || q->msges == NULL || q->ptrRead == q->ptrWrite) {
        spin_unlock(&q->lock);
        return SYS_MBOX_EMPTY;
    }

    if (msg != NULL) {
        *msg = q->msges[q->ptrRead];
    }
    q->ptrRead = (q->ptrRead + 1) % q->size;
    spin_unlock(&q->lock);

    return ERR_OK;
}

// mmm, trash
void *sio_open(u8_t devnum) { return 0; }
u32_t sio_write(void *fd, const u8_t *data, u32_t len) { return 0; }
void sio_send(u8_t c, void *fd) {}
u8_t sio_recv(void *fd) { return 0; }
u32_t sio_read(void *fd, u8_t *data, u32_t len) { return 0; }
u32_t sio_tryread(void *fd, u8_t *data, u32_t len) { return 0; }
}
