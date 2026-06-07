#include "task/poll.h"
#include "proto.hpp"
#include "task/scheduler.h"

static volatile uint64_t g_poll_event_generation = 1;
static spin_t            g_poll_waiters_lock     = SPIN_INIT;
static tcb_t             g_poll_waiters[256];

uint64_t poll_event_generation(void)
{
    return __atomic_load_n(&g_poll_event_generation, __ATOMIC_ACQUIRE);
}

void poll_notify_event(void)
{
    __atomic_add_fetch(&g_poll_event_generation, 1, __ATOMIC_RELEASE);

    tcb_t wake_list[256];
    size_t wake_count = 0;

    spin_lock(&g_poll_waiters_lock);
    for (size_t i = 0; i < sizeof(g_poll_waiters) / sizeof(g_poll_waiters[0]); i++)
    {
        tcb_t task = g_poll_waiters[i];
        if (task != NULL) wake_list[wake_count++] = task;
    }
    spin_unlock(&g_poll_waiters_lock);

    for (size_t i = 0; i < wake_count; i++)
        scheduler_wake_task(wake_list[i]);
}

void poll_waiter_arm(void)
{
    tcb_t task = get_current_task();
    if (task == NULL) return;

    int free_slot = -1;
    spin_lock(&g_poll_waiters_lock);
    for (size_t i = 0; i < sizeof(g_poll_waiters) / sizeof(g_poll_waiters[0]); i++)
    {
        if (g_poll_waiters[i] == task)
        {
            spin_unlock(&g_poll_waiters_lock);
            return;
        }
        if (g_poll_waiters[i] == NULL && free_slot < 0) free_slot = (int)i;
    }
    if (free_slot >= 0) g_poll_waiters[free_slot] = task;
    spin_unlock(&g_poll_waiters_lock);
}

void poll_waiter_disarm(void)
{
    tcb_t task = get_current_task();
    if (task == NULL) return;

    spin_lock(&g_poll_waiters_lock);
    for (size_t i = 0; i < sizeof(g_poll_waiters) / sizeof(g_poll_waiters[0]); i++)
    {
        if (g_poll_waiters[i] == task)
        {
            g_poll_waiters[i] = NULL;
            break;
        }
    }
    spin_unlock(&g_poll_waiters_lock);
}

uint32_t epoll_to_poll_comp(uint32_t epoll_events) {
    uint32_t poll_events = 0;

    if (epoll_events & EPOLLIN) { poll_events |= POLLIN; }
    if (epoll_events & EPOLLOUT) { poll_events |= POLLOUT; }
    if (epoll_events & EPOLLPRI) { poll_events |= POLLPRI; }
    if (epoll_events & EPOLLERR) { poll_events |= POLLERR; }
    if (epoll_events & EPOLLHUP) { poll_events |= POLLHUP; }
    if (epoll_events & EPOLLRDNORM) { poll_events |= POLLRDNORM | POLLIN; }
    if (epoll_events & EPOLLRDBAND) { poll_events |= POLLRDBAND; }
    if (epoll_events & EPOLLWRNORM) { poll_events |= POLLWRNORM | POLLOUT; }
    if (epoll_events & EPOLLWRBAND) { poll_events |= POLLWRBAND; }
    if (epoll_events & EPOLLMSG) { poll_events |= POLLMSG; }
    if (epoll_events & EPOLLRDHUP) { poll_events |= POLLRDHUP; }

    return poll_events;
}

uint32_t poll_to_epoll_comp(uint32_t poll_events) {
    uint32_t epoll_events = 0;

    if (poll_events & POLLIN) { epoll_events |= EPOLLIN; }
    if (poll_events & POLLOUT) { epoll_events |= EPOLLOUT; }
    if (poll_events & POLLPRI) { epoll_events |= EPOLLPRI; }
    if (poll_events & POLLERR) { epoll_events |= EPOLLERR; }
    if (poll_events & POLLHUP) { epoll_events |= EPOLLHUP; }
    if (poll_events & POLLRDNORM) { epoll_events |= EPOLLRDNORM | EPOLLIN; }
    if (poll_events & POLLRDBAND) { epoll_events |= EPOLLRDBAND; }
    if (poll_events & POLLWRNORM) { epoll_events |= EPOLLWRNORM | EPOLLOUT; }
    if (poll_events & POLLWRBAND) { epoll_events |= EPOLLWRBAND; }
    if (poll_events & POLLMSG) { epoll_events |= EPOLLMSG; }
    if (poll_events & POLLRDHUP) { epoll_events |= EPOLLRDHUP; }

    return epoll_events;
}

struct pollfd *select_add(struct pollfd **comp, size_t *compIndex, size_t *complength, int fd,
                          int events) {
    if ((*compIndex + 1) * sizeof(struct pollfd) >= *complength) {
        *complength *= 2;
        *comp        = (struct pollfd*)realloc(*comp, *complength);
    }

    (*comp)[*compIndex].fd      = fd;
    (*comp)[*compIndex].events  = events;
    (*comp)[*compIndex].revents = 0;

    return &(*comp)[(*compIndex)++];
}

bool select_bitmap(const uint8_t *map, int index) {
    int div = index / 8;
    int mod = index % 8;
    return map[div] & (1 << mod);
}

void select_bitmap_set(uint8_t *map, int index) {
    int div   = index / 8;
    int mod   = index % 8;
    map[div] |= 1 << mod;
}
