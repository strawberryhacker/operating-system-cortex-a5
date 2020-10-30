/// Copyright (C) strawberryhacker 

#include <cinnamon/sched.h>
#include <cinnamon/print.h>
#include <cinnamon/pit.h>
#include <cinnamon/apic.h>
#include <cinnamon/list.h>
#include <cinnamon/thread.h>
#include <cinnamon/apic.h>
#include <cinnamon/interrupt.h>
#include <cinnamon/atomic.h>
#include <cinnamon/panic.h>
#include <cinnamon/cpu_timer.h>
#include <regmap.h>

#define PIT_IRQ 3

/// Each CPU has a private runqueue
struct rq rq;

void core_sched(struct rq* rq, u32 reschedule);

/// Array for the scheduling classes
#define CLASS_CNT 2

const struct sched_class* sched_classes[CLASS_CNT];

/// Returning the scheduling class based on the sched class number gotten from
/// the thread flags
const struct sched_class* get_sched_class(u32 class_num)
{
    if (class_num >= CLASS_CNT) {
        panic("Update the thread flags to the number of schedulers");
    }
    return sched_classes[class_num];
}

/// Enqueus a thread into the running queue of a scheduling class
void sched_enqueue_thread(struct thread* thread)
{
    if (thread->class == NULL) {
        panic("Thread does not have a class");
    }
    thread->class->enqueue(thread, &rq);
}

/// FIX THIS
void enqueue_sleeping_threads(struct rq* rq)
{
    u64 tick = rq->time.tick;

    struct list_node* list = &rq->sleep_list;

    while (list->next != list) {
        // Check if the thread tick to wake is bigger than the tick
        struct thread* t = list_get_entry(list->next, struct thread, node);

        if (t->tick_to_wake <= tick) {
            list_delete_first(list);
            t->class->enqueue(t, rq);
        } else {
            break;
        }
    }

    // Fix the rq->tick_to_wake to it point to the next tick to wake if any
    if (list->next != list) {
        struct thread* t = list_get_entry(list->next, struct thread, node);
        rq->time.tick_to_wake = t->tick_to_wake;
    } else {
        rq->time.tick_to_wake = 0;
    }
}

/// Scheduler interrupt being called every ms
void cpu_tick_handler(void)
{
    cpu_timer_clear_flags();

    // This is called within the IRQ. This will update the next_thread. The rest
    // of the IRQ routine (which will run directly after this function) will 
    // do the acctual context switching if the `new` is non zero
    core_sched(&rq, 0);
}

/// Scheduling classes
extern const struct sched_class rt_class;
extern const struct sched_class idle_class;

/// Called by the scheduler interrupt and will return the next thread to run on 
/// the CPU given the CPUs private runqueue structure
static inline struct thread* core_pick_next(struct rq* rq)
{
    const struct sched_class* class;
    u32 i = 0;
    for (class = &rt_class; class; class = class->next) {
        // Ask for a new thread in the scheduling class 
        struct thread* thread = class->pick_next(rq);
        if (thread) {
            return thread;
        }
    }
    print("Core sched error\n");
    while (1);
}

void sched_save_runtime(struct rq* rq)
{
    struct list_node* it;
    list_iterate(it, &rq->thread_list) {
        struct thread* t = list_get_entry(it, struct thread, thread_node);

        t->window_runtime = t->curr_runtime;
        t->curr_runtime = 0;
    }
}

/// Core scheduler. This must be called inside either the IRQ interrupt or the
/// SVC interrupt. These interrupts have special mechanisms for doing a context 
/// switch
void core_sched(struct rq* rq, u32 reschedule)
{
    // If we have a early yield the value is NOT 1000
    u32 runtime;
    if (reschedule) {
        runtime = cpu_timer_get_value() / 11;
    } else {
        runtime = 1000;
    }

    rq->time.tick += runtime;
    rq->time.tick_window += runtime;
    rq->curr->curr_runtime += runtime;

    if (rq->time.tick_window > 1000 * 1000) {
        rq->time.window = rq->time.tick_window;
        rq->time.tick_window = 0;
        // Its time to recalculate the runtimes
        sched_save_runtime(rq);
    }

    if (rq->time.tick > rq->time.tick_to_wake && rq->time.tick_to_wake) {
        enqueue_sleeping_threads(rq);
    }

    struct thread* new = core_pick_next(rq);
    if (new != rq->curr) {
        rq->next = new;
    }
}

/// Adds a thread to the rq thread list
void sched_add_thread(struct thread* thread)
{
    list_add_first(&thread->thread_node, &rq.thread_list);
}

/// Initializes the main runqueue structure 
void sched_init_rq(struct rq* rq)
{
    // Initialize all the lists 
    list_init(&rq->thread_list);
    list_init(&rq->sleep_list);

    rq->curr = NULL;
    rq->next = NULL;

    rq->sched_enable = 1;

    // Init time
    rq->time.tick = 0;
    rq->time.tick_to_wake = 0;
    rq->time.tick_window = 0;

    // Initialize the private data for all the scheduling classes 
    const struct sched_class* class;
    u32 class_index = 0;
    for (class = &rt_class; class; class = class->next) {
        if (class_index >= CLASS_CNT) {
            panic("Class array is wrong");
        }
        class->init(rq);
        sched_classes[class_index++] = class;
    }
}

/// This is the IDLE thread which is run when no other scheduling class can
/// offer any new thread. This might not need to occupy a full timeslice but
/// should probably wait for a signal
static u32 idle_func(void* args)
{
    while (1) {

    }

    return 1;
}

static void add_idle(struct rq* rq)
{
    struct thread* idle_thread =
        create_kernel_thread(idle_func, 500, "idle", NULL, SCHED_IDLE);
}

static inline void reschedule(void);

/// Early init routine for the scheduler 
static inline void sched_early_init(void)
{
    print("Sched starting...\n");

    // Initializes the CPU master runqueue 
    sched_init_rq(&rq);
}

/// Initialized the scheduler for the CPU
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

/// Returns the main kernel tick value 
u64 get_kernel_tick(void)
{
    return rq.time.tick;
}

/// Returns the current thread running on this CPU
struct thread* get_curr_thread(void)
{
    return rq.curr;
}   

/// Adds a thread to the sorted delay list for a given runqueue. The thread with
/// the shortest time left to sleep will be placed in front of the queue. This
/// will modify the threads node pointers; therefore the thread must NOT exist in
/// any list at this point
void add_sleep_list(struct thread* thread, struct rq* rq)
{
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

/// Force a reschedule by writing directly to the APIC
static inline void reschedule(void)
{
    apic_force(PIT_IRQ);
    asm volatile ("dmb" : : : "memory");
    asm volatile ("dsb" : : : "memory");
    asm volatile ("isb" : : : "memory");
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

/// Functions for temporarily stopping the scheduler from executing. The timers 
/// and interrupts will still run
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

/// This will put the current running thread into the sleep queue for a number of 
/// microseconds
void sched_thread_sleep(u32 us)
{
    // Get the current thread
    struct thread* curr = get_curr_thread();
    u64 tick = get_kernel_tick();
    
    curr->tick_to_wake = tick + (u64)us;
    curr->class->dequeue(curr, &rq);
    add_sleep_list(curr, &rq);
    
    core_sched(&rq, 1);
}
