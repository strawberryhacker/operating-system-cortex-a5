/// Copyright (C) strawberryhacker

#ifndef THREAD_H
#define THREAD_H

#include <cinnamon/types.h>
#include <cinnamon/list.h>
#include <cinnamon/mm.h>

struct sched_class;

/// Thread flags
#define SCHED_RT   0b000
#define SCHED_IDLE  0b001

#define THREAD_MAX_NAME 32

struct thread {
    // The stack pointer has to be the first element (must be first)
    u32* sp;
    
    // Hold the setup for the process memory space (must be second)
    struct thread_mm* mm;

    // =========================================================================
    // Do NOT modify anything above this line
    // =========================================================================

    
    u32 page_cnt;

    // Base address for the stack
    u32* stack_base;

    // Tick to wake 
    u64 tick_to_wake;

    // Holds the last windows runtime in us
    u32 window_runtime;
    u32 curr_runtime;

    char name[THREAD_MAX_NAME];

    /// Not the same as the ASID
    u32 pid;

    /// Node to attach the thread to a running / blocked / sleep queue
    struct list_node node;

    /// Keep a pointer to its scheduling class
    const struct sched_class* class;

    // Pointer to the process and list all threads within a process 
    struct thread* process;
    struct list_node thread_group;

    /// Contains all the threads in the system
    struct list_node thread_node;

};

struct thread* create_kernel_thread(u32 (*func)(void *), u32 stack_size, 
    const char* name, void* args, u32 flags);

struct thread* create_process(u32 (*func)(void *), u32 stack_size,
    const char* name, void* args, u32 flags);

struct thread* create_thread(u32 (*func)(void *), u32 stack_size, 
    const char* name, void* args, u32 flags);

void map_in_code(struct page* code_page, u32 pages, struct thread* t);
void curr_thread_add_pages(struct page* page, u32 pages);
struct thread_mm* get_curr_mm_process(void);

#endif
