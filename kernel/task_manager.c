/// Copyright (C) strawberryhacker

#include <cinnamon/task_manager.h>
#include <cinnamon/thread.h>
#include <cinnamon/sched.h>
#include <cinnamon/syscall.h>
#include <cinnamon/mm.h>
#include <cinnamon/cache.h>

void print_thread_header(void)
{
    print("%3s %-16s %5s %8s\n", "PID", "NAME", "CPU%", "MEM");
}

void print_thread_stats(u32 pid, const char* name, u8 percent, u8 frac, u32 mem)
{
    print("%3d %-16s %2d.%02d %8d\n", pid, name, percent, frac, mem);
}

void print_cpu_usage(u8 cpu_usage)
{
    u8 bars = cpu_usage / 5;
    u8 space = 20 - bars;

    print("CPU [" GREEN);
    for (u32 i = 0; i < bars; i++) {
        print("|");
    }
    print(NORMAL "%*s %3d%%]\n", space, "", cpu_usage);
}

/// Prints the memory usage in the system
void print_mem_usage(u32 total, u32 used)
{
    print("TOTAL %d\n", total);
    print("USED  %d\n", used);
    u8 bars = (used / (total / 100)) / 5;
    u8 space = 20 - bars;

    print("MEM [" BLUE);
    for (u32 i = 0; i < bars; i++) {
        print("|");
    }
    print(NORMAL "%*s %3d%%]\n", space, "", used / (total / 100));
}

extern struct rq rq;

u32 thread(void* args)
{
    while (1) {
        syscall_thread_sleep(10000);
        syscall_sbrk(1);
    }
}

u32 task_manager(void* args)
{
    //dcache_clean();
    syscall_create_thread(thread, 500, "allocthread", NULL, SCHED_RT);
    while (1) {
        syscall_thread_sleep(1000000);

        syscall_sbrk(33000000);

        // Print the task manager header
        u32 p = (100 * rq.idle_rq.idle->window_runtime) / rq.time.window;

        print_cpu_usage(100 - p);
        print_mem_usage(mm_get_total(), mm_get_total_used());
        print_thread_header();

        // Print runtime stats for all the threads
        struct list_node* it;
        list_iterate(it, &rq.thread_list) {
                struct thread* t = list_get_entry(it, struct thread, thread_node);

            u32 w_runtime = t->window_runtime;
            u32 w = rq.time.window;
            u32 percent = (100 * w_runtime) / w;
            u32 fraction = ((100 * w_runtime) / (w / 100)) - (percent * 100);

            u32 mem_kib = t->page_cnt * 4;

            print_thread_stats(t->pid, t->name, percent, fraction, mem_kib);
        }
        print("\n");
    }
}   

void task_manager_init(void)
{
    create_process(task_manager, 500, "taskmgmt", "HELLO", SCHED_RT);
}
