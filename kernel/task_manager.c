// Copyright (C) strawberryhacker

#include <citrus/task_manager.h>
#include <citrus/thread.h>
#include <citrus/sched.h>
#include <citrus/syscall.h>
#include <citrus/mm.h>
#include <citrus/cache.h>

#define BARS 20

void print_thread_header(void)
{
    print_task("%3s %-16s %5s %11s\n", "PID", "NAME", "CPU%", "MEM");
}

void print_thread_stats(u32 pid, const char* name, u8 percent, u8 frac, u32 mem)
{
    print_task("%3d %-16s %2d.%02d %8d KB\n", pid, name, percent, frac, mem);
}

void print_cpu_usage(u8 cpu_usage)
{
    u8 bars = (BARS * cpu_usage) / 100;
    u8 space = BARS - bars;

    print_task("CPU [" GREEN);
    for (u32 i = 0; i < bars; i++) {
        print_task("|");
    }
    print_task(NORMAL "%*s %3d%%]\n", space, "", cpu_usage);
}

// Prints the memory usage in the system
void print_mem_usage(u32 total, u32 used)
{
    u8 bars = (used * BARS) / total;
    u8 space = BARS - bars;

    print_task("MEM [" BLUE);
    for (u32 i = 0; i < bars; i++) {
        print_task("|");
    }
    print_task(NORMAL "%*s %3d%%]\n", space, "", used / (total / 100));
}

extern struct rq rq;

i32 task_manager(void* args)
{
    // Setup the task management
    struct list_node* it;
    list_iterate(it, &rq.thread_list) {
        struct thread* t = list_get_entry(it, struct thread, thread_node);
        t->last_runtime = t->runtime;
    }

    while (1) {
        syscall_thread_sleep(1000);

        // Print the task manager header
        struct thread* idle = rq.idle_rq.idle;
        u32 idle_percent = (idle->runtime - idle->last_runtime) / 10000;

        print_cpu_usage(100 - idle_percent);
        print_mem_usage(mm_get_total(), mm_get_total_used());

        // Print the thread header
        print_thread_header();

        // Print runtime stats for all the threads
        struct list_node* it;
        list_iterate(it, &rq.thread_list) {
            struct thread* t = list_get_entry(it, struct thread, thread_node);

            u32 runtime = t->runtime - t->last_runtime;
            u32 percent = runtime / 10000;
            u32 fraction = (runtime / 100) - (percent * 100);
            u32 mem_kib = t->page_cnt * 4;

            print_thread_stats(t->pid, t->name, percent, fraction, mem_kib);
            t->last_runtime = t->runtime;
        }
        print_task("\n");
    }
}   

void task_manager_init(void)
{
    create_kthread(task_manager, 500, "taskmgmt", "HELLO", SCHED_RT);
}
