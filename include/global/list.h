#include "../include.h"
#define NULL 0

#define container_of(ptr, type, member)                                                            \
    ({                                                                                             \
        decltype(((type *)0)->member) *p = (ptr);                                                  \
        (type *)((unsigned long)p - (unsigned long)&(((type *)0)->member));                        \
    })

struct List {
    struct List *prev;
    struct List *next;
};

static inline void list_init(struct List *list) {
    list->prev = list;
    list->next = list;
}

static inline void list_add_to_behind(struct List *entry, struct List *ne_w) ////add to entry behind
{
    ne_w->next       = entry->next;
    ne_w->prev       = entry;
    ne_w->next->prev = ne_w;
    entry->next      = ne_w;
}

static inline void list_add_to_before(struct List *entry, struct List *ne_w) ////add to entry behind
{
    ne_w->next        = entry;
    entry->prev->next = ne_w;
    ne_w->prev        = entry->prev;
    entry->prev       = ne_w;
}

static inline void list_del(struct List *entry) {
    entry->next->prev = entry->prev;
    entry->prev->next = entry->next;
}

static inline long list_is_empty(struct List *entry) {
    if (entry == entry->next && entry->prev == entry)
        return 1;
    else
        return 0;
}

static inline struct List *list_prev(struct List *entry) {
    if (entry->prev != (struct List *)NULL)
        return entry->prev;
    else
        return (struct List *)NULL;
}

static inline struct List *list_next(struct List *entry) {
    if (entry->next != (struct List *)NULL)
        return entry->next;
    else
        return (struct List *)NULL;
}
