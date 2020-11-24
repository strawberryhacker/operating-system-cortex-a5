// Copyright (C) strawberryhacker 

#include <citrus/sched.h>
#include <citrus/print.h>
#include <citrus/pit.h>
#include <citrus/apic.h>
#include <citrus/list.h>
#include <citrus/thread.h>
#include <citrus/apic.h>
#include <citrus/interrupt.h>
#include <citrus/atomic.h>
#include <citrus/panic.h>
#include <citrus/cpu_timer.h>
#include <citrus/kmalloc.h>
#include <citrus/regmap.h>
#include <citrus/pid.h>

// Each CPU has a private runqueue
struct rq rq;

// Array for the scheduling classes
#define CLASS_CNT 4

const struct sched_class* sched_classes[CLASS_CNT];

// Returning the scheduling class based on the sched class number gotten from
// the thread flags
const struct sched_class* get_sched_class(u32 class_num)
{   
    assert(class_num < CLASS_CNT);
    return sched_classes[class_num];
}

// Enqueus a thread into the running queue of a scheduling class
void sched_enqueue_thread(struct thread* thread)
{
    assert(thread->class);
    thread->state = THREAD_RUNNING;
    thread->class->enqueue(thread, &rq);
}

// Dequeues a thread from its runqueue
void sched_dequeue_thread(struct thread* thread)
{
    assert(thread->class);
    thread->class->dequeue(thread, &rq);
}

// Checks the sleep queue and enqueues all thread with expired delay
void enqueue_sleeping_threads(struct rq* rq)
{
    u64 tick = rq->time.tick;

    struct list_node* list = &rq->sleep_list;
    while (list->next != list) {
        // Check if the thread tick to wake is bigger than the tick
        struct thread* t = list_get_entry(list->next, struct thread, node);

        if (t->tick_to_wake > tick) {
            break;
        }
        

        list_delete_first(list);
        t->class->enqueue(t, rq);
    }

    // Fix the rq->tick_to_wake to it point to the next tick to wake - if any
    if (list->next != list) {
        struct thread* t = list_get_entry(list->next, struct thread, node);
        rq->time.tick_to_wake = t->tick_to_wake;
    } else {
        rq->time.tick_to_wake = 0;
    }
}

// Core scheduler
void core_sched(struct rq* rq, u32 reschedule);

// Scheduler interrupt being called every ms
void cpu_tick_handler(void)
{
    cpu_timer_clear_flags();

    // This is called within the IRQ. This will update the next_thread. The rest
    // of the IRQ routine (which will run directly after this function) will 
    // do the acctual context switching if the `new` is non zero
    core_sched(&rq, 0);
}

// Scheduling classes
extern const struct sched_class rt_class;
extern const struct sched_class idle_class;

// Called by the scheduler interrupt and will return the next thread to run on 
// the CPU given the CPUs private runqueue structure
static inline struct thread* core_pick_next(struct rq* rq)
{
    const struct sched_class* class;

    for (class = &rt_class; class; class = class->next) {
        struct thread* thread = class->pick_next(rq);
        if (thread)
            return thread;
    }
    panic("Core scheduler hard fault");
    return NULL;
}

// Core scheduler. This must be called inside either the IRQ interrupt or the
// SVC interrupt. These interrupts have special mechanisms for doing a context 
// switch
void core_sched(struct rq* rq, u32 reschedule)
{
    u32 runtime;
    if (reschedule) {
        runtime = cpu_timer_get_value_us();
        cpu_timer_reset();
    } else {
        runtime = SCHED_SLICE;
    }

    rq->time.tick += runtime;
    rq->curr->runtime += runtime;

    // Enqueue expired delays
    if (rq->time.tick > rq->time.tick_to_wake && rq->time.tick_to_wake)
        enqueue_sleeping_threads(rq);

    struct thread* new = core_pick_next(rq);

    // The context switch will not happend if the thread is the same
    if (new != rq->curr)
        rq->next = new;
}

// Adds a thread to the rq thread list
void sched_add_thread(struct thread* thread)
{
    list_add_first(&thread->thread_node, &rq.thread_list);
}

// Initializes the main runqueue structure 
void sched_init_rq(struct rq* rq)
{
    // Initialize all the lists 
    list_init(&rq->thread_list);
    list_init(&rq->sleep_list);

    rq->curr = NULL;
    rq->next = NULL;
    rq->lazy_fpu = NULL;

    rq->sched_enable = 1;

    // Init time
    rq->time.tick = 0;
    rq->time.tick_to_wake = 0;
    rq->time.tick_window = 0;

    // Initialize the private data for all the scheduling classes 
    const struct sched_class* class;
    u32 class_index = 0;
    for (class = &rt_class; class; class = class->next) {
        class->init(rq);
        sched_classes[class_index++] = class;
    }
}

// Sets the current lazy FPU user
void sched_set_lazy_fpu_user(struct thread* thread)
{
    rq.lazy_fpu = thread;
}

// Gets the current lazy FPU user
struct thread* sched_get_lazy_fpu_user(void)
{
    return rq.lazy_fpu;
}

// This is the IDLE thread which is run when no other scheduling class can
// offer any new thread. This might not need to occupy a full timeslice but
// should probably wait for a signal
//
// TODO this should yield on a external signal
static i32 idle_func(void* args)
{
    while (1);
    return 1;
}

// Adds the IDLE thread to the system
static void add_idle(struct rq* rq)
{
    struct thread* idle_thread =
        create_kthread(idle_func, 500, "idle", NULL, SCHED_IDLE);
}

// Early init routine for the scheduler 
static inline void sched_early_init(void)
{
    // Initializes the CPU master runqueue 
    sched_init_rq(&rq);

    // Initialize the PID interface
    pid_init();
}

// Initialized the scheduler for the CPU
void sched_init(void)
{
    sched_early_init();

    add_idle(&rq);

    // This should not be necessary
    irq_disable();

    // Enable APIC for the CPU timer
    apic_add_handler(36, cpu_tick_handler);
    apic_enable(36);

    cpu_timer_init();
}

// Returns the main kernel tick value 
u64 get_kernel_tick(void)
{
    return rq.time.tick;
}

// Returns the current thread running on this CPU
struct thread* get_curr_thread(void)
{
    return rq.curr;
}   

// Adds a thread to the sorted delay list for a given runqueue. The thread with
// the shortest time left to sleep will be placed in front of the queue. This
// will modify the threads node pointers; therefore the thread must NOT exist in
// any list at this point
void add_sleep_list(struct thread* thread, struct rq* rq)
{
    thread->state = THREAD_SLEEP;
    u64 curr_tick = thread->tick_to_wake;

    // Conditionally update the rq.tick_to_wake 
    if (curr_tick < rq->time.tick_to_wake || (rq->time.tick_to_wake == 0)) {
        rq->time.tick_to_wake = curr_tick;
    }

    // Place the thread in the ordered delay list 
    struct list_node* node;
    list_iterate(node, &rq->sleep_list) {
        struct thread* t = list_get_entry(node, struct thread, node);
        
        if (t->tick_to_wake >= curr_tick) {
            list_add_before(&thread->node, &t->node);
            return;
        }
    }

    // This is the longest delay 
    list_add_last(&thread->node, &rq->sleep_list);
}

void print_queue(struct list_node* queue)
{
    struct list_node* node;
    list_iterate(node, queue) {
        struct thread* t = list_get_entry(node, struct thread, node);
        print("%d ", t->pid);
    }
    print("\n");
}

// Functions for temporarily stopping the scheduler from executing. The timers 
// and interrupts will still run
u32 sched_disable(void)
{
    u32 i = rq.sched_enable;
    rq.sched_enable = 0;
    return i;
}

void sched_enable(u32 i)
{
    rq.sched_enable = i;
}

// This will put the current running thread into the sleep queue for a number of 
// microseconds
void sched_thread_sleep(u32 ms)
{
    // Get the current thread
    struct thread* curr = get_curr_thread();
    u64 tick = get_kernel_tick();
    
    curr->tick_to_wake = tick + (u64)(ms * 1000);
    curr->class->dequeue(curr, &rq);
    add_sleep_list(curr, &rq);
    
    core_sched(&rq, 1);
}

u8 sched_kill_thread(struct thread* thread)
{
    list_delete_node(&thread->thread_group);
    list_delete_node(&thread->thread_node);

    thread->class->dequeue(thread, &rq);

    kfree(thread);

    return 1;
}

struct rq* get_rq(void)
{
    return &rq;
}
