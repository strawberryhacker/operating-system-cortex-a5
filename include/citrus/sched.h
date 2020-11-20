#ifndef SCHED_H
#define SCHED_H

#include <citrus/types.h>
#include <citrus/list.h>

// Main real-time runqueue 
struct rt_rq {
    struct list_node queue;
};

struct fair_rq {
    struct list_node queue;
};

struct back_rq {
    struct list_node queue;
};

// Main idle runqueue 
struct idle_rq {
    struct thread* idle;
};

/// The scheduler time base will vary depending on the time slice given to each 
/// thread. Ideally the scheduler will preempt the execution every millisecond
/// incrementing the tick variable by 1.000. Any early yield or preemtion will 
/// result in a timeslice between 0 and 1.000 micro ticks
struct time {
    volatile u64 tick;
    volatile u64 tick_to_wake;

    // Reset every second i.e. when the tick_window is bigger than 1M us
    volatile u32 tick_window;
    volatile u32 window;
};

/// Main CPU runqueue 
struct rq {
    // Holds pointers to the current and next thread to run on the CPU. The 
    // `next` must be NULL if no context switch is required. If this field is 
    // non-zero any interrupt will trigger a context switch. The `curr` thread
    // is allways updated by the context switch routine

    struct thread* next; // Must be first  
    struct thread* curr; // Must be second

    // This points to either NULL or the current lazy FPU thread. This means 
    // that the thread still has its VPU registers S0-S31 in the register bank
    struct thread* lazy_fpu;

    // =========================================================================
    // Do NOT modify anything above this line !!!
    // =========================================================================

    // Private runqueue data structure for the scheduling classes 
    struct rt_rq rt_rq;
    struct fair_rq fair_rq;
    struct back_rq back_rq;
    struct idle_rq idle_rq;

    // List all the threads in the system 
    struct list_node thread_list;

    // Keep a sorted list of sleeping threads 
    struct list_node sleep_list;

    struct time time;

    u32 sched_enable;
};

/// Each scheduling class initializes its own struct sched_class. This will 
/// provide all the necessary functions for thread operation within that 
/// scheduling class. The four main scheduling classes real time, application, 
/// background and idle is defined in each own c file 
struct sched_class {
    const struct sched_class* next;

    void (*enqueue)(struct thread* thread, struct rq* rq);
    void (*dequeue)(struct thread* thread, struct rq* rq);

    // Called by the core scheduler 
    struct thread* (*pick_next)(struct rq* rq);

    void (*init)(struct rq* rq);
};

void sched_init(void);
void sched_start(void);

/// Put the current runing thread to sleep for a number of us
void sched_thread_sleep(u32 ms);

// Currently this is a single core operating system so we only use one runqueue
struct rq* get_rq(void);

u32 sched_disable(void);

void sched_enable(u32 i);

const struct sched_class* get_sched_class(u32 class_num);

void sched_enqueue_thread(struct thread* thread);

void sched_dequeue_thread(struct thread* thread);

u64 get_kernel_tick(void);

struct thread* get_curr_thread(void);

/// Adds a thread to the global thread list
void sched_add_thread(struct thread* thread);

/// Functions fo lazy use of the FPU context
void sched_set_lazy_fpu_user(struct thread* thread);

struct thread* sched_get_lazy_fpu_user(void);

u8 sched_kill_thread(struct thread* thread);

#endif
