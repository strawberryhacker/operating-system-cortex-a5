// Copyright (C) strawberryhacker

#include <citrus/types.h>
#include <citrus/sched.h>
#include <citrus/print.h>
#include <citrus/list.h>
#include <citrus/thread.h>
#include <citrus/interrupt.h>
#include <citrus/atomic.h>
#include <stddef.h>

void fair_init(struct rq* rq)
{
    //print("Init fair\n");
    struct fair_rq* fair_rq = &rq->fair_rq;

    list_init(&fair_rq->queue);
}

void fair_enqueue(struct thread* thread, struct rq* rq)
{
    u32 flags = __atomic_enter();
    list_add_last(&thread->node, &rq->fair_rq.queue);
    __atomic_leave(flags);
}

void fair_dequeue(struct thread* thread, struct rq* rq)
{
    u32 flags = __atomic_enter();
    list_delete_node(&thread->node);
    __atomic_leave(flags);
}

struct thread* fair_pick_next(struct rq* rq)
{
    if (list_is_empty(&rq->fair_rq.queue)) {
        return NULL;
    }
    struct list_node* tmp = list_get_first(&rq->fair_rq.queue);
    
    // Lock the runqueue while picking next
    u32 flags = __atomic_enter();
    list_delete_first(&rq->fair_rq.queue);
    list_add_last(tmp, &rq->fair_rq.queue);
    __atomic_leave(flags);

    return list_get_entry(tmp, struct thread, node);
}

extern const struct sched_class back_class;
const struct sched_class fair_class = {
        .next = &back_class,
        .init = &fair_init,
        .enqueue = &fair_enqueue,
        .dequeue = &fair_dequeue,
        .pick_next = &fair_pick_next
};
