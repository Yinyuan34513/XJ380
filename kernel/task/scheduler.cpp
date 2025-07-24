#include "task/pcb.h"
#include "smp/smp.h"
#include "lock_queue.h"

extern lock_queue *pcb_group_queue;
extern XSK_SMP_INFO *xsi;

size_t add_task(pcb_t task) {
    task->group_index = queue_enqueue(pcb_group_queue, task);

}
