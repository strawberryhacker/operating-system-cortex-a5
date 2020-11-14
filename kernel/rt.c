// Copyright (C) strawberryhacker

#include <citrus/types.h>
#include <citrus/sched.h>
#include <citrus/print.h>
#include <citrus/list.h>
#include <citrus/thread.h>
#include <citrus/interrupt.h>
#include <citrus/atomic.h>
#include <stddef.h>

void rt_init(struct rq* rq)
{
    //print("Init RT\n");
    struct rt_rq* rt_rq = &rq->rt_rq;

    list_init(&rt_rq->queue);
}

void rt_enqueue(struct thread* thread, struct rq* rq)
{
    u32 flags = __atomic_enter();
    list_add_last(&thread->node, &rq->rt_rq.queue);
    __atomic_leave(flags);
}

void rt_dequeue(struct thread* thread, struct rq* rq)
{
    u32 flags = __atomic_enter();
    list_delete_node(&thread->node);
    __atomic_leave(flags);
}

struct thread* rt_pick_next(struct rq* rq)
{
    if (list_is_empty(&rq->rt_rq.queue)) {
        return NULL;
    }
    struct list_node* tmp = list_get_first(&rq->rt_rq.queue);
    
    // Lock the runqueue while picking next
    u32 flags = __atomic_enter();
    list_delete_first(&rq->rt_rq.queue);
    list_add_last(tmp, &rq->rt_rq.queue);
    __atomic_leave(flags);

    return list_get_entry(tmp, struct thread, node);
}

extern const struct sched_class fair_class;
const struct sched_class rt_class = {
        .next = &fair_class,
        .init = &rt_init,
        .enqueue = &rt_enqueue,
        .dequeue = &rt_dequeue,
        .pick_next = &rt_pick_next
};
