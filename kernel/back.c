// Copyright (C) strawberryhacker

#include <citrus/types.h>
#include <citrus/sched.h>
#include <citrus/print.h>
#include <citrus/list.h>
#include <citrus/thread.h>
#include <citrus/interrupt.h>
#include <citrus/atomic.h>
#include <stddef.h>

void back_init(struct rq* rq)
{
    //print("Init back\n");
    struct back_rq* back_rq = &rq->back_rq;

    list_init(&back_rq->queue);
}

void back_enqueue(struct thread* thread, struct rq* rq)
{
    u32 flags = __atomic_enter();
    list_add_last(&thread->node, &rq->back_rq.queue);
    __atomic_leave(flags);
}

void back_dequeue(struct thread* thread, struct rq* rq)
{
    u32 flags = __atomic_enter();
    list_delete_node(&thread->node);
    __atomic_leave(flags);
}

struct thread* back_pick_next(struct rq* rq)
{
    if (list_is_empty(&rq->back_rq.queue)) {
        return NULL;
    }
    struct list_node* tmp = list_get_first(&rq->back_rq.queue);
    
    // Lock the runqueue while picking next
    u32 flags = __atomic_enter();
    list_delete_first(&rq->back_rq.queue);
    list_add_last(tmp, &rq->back_rq.queue);
    __atomic_leave(flags);

    return list_get_entry(tmp, struct thread, node);
}

extern const struct sched_class idle_class;
const struct sched_class back_class = {
        .next = &idle_class,
        .init = &back_init,
        .enqueue = &back_enqueue,
        .dequeue = &back_dequeue,
        .pick_next = &back_pick_next
};
