/// Copyright (C) strawberryhacker 

#include <cinnamon/types.h>
#include <cinnamon/sprint.h>
#include <cinnamon/apic.h>
#include <cinnamon/print.h>
#include <cinnamon/buddy_alloc.h>
#include <cinnamon/cache.h>
#include <cinnamon/mm.h>
#include <cinnamon/benchmark.h>
#include <cinnamon/thread.h>
#include <cinnamon/sched.h>
#include <cinnamon/page_alloc.h>
#include <cinnamon/pt_entry.h>
#include <cinnamon/syscall.h>
#include <cinnamon/interrupt.h>
#include <cinnamon/atomic.h>
#include <cinnamon/kmalloc.h>
#include <cinnamon/loader.h>
#include <cinnamon/panic.h>
#include <cinnamon/elf.h>
#include <cinnamon/gpio.h>
#include <cinnamon/mmc.h>

#include <regmap.h>
#include <stdarg.h>
#include <stdalign.h>

extern struct rq rq;
extern const struct sched_class rt_class;

/// Early initialization for the kernel
void early_init(void)
{
    apic_init();

    // The loader is in control of the kernel download serial line and 
    // should be initialized first in order to access the bootloader. 
    loader_init();

    // Enable interrupt now to support reboot
    irq_disable();
    async_abort_enable();

    icache_enable();
    dcache_enable();

    icache_invalidate();
    dcache_clean_invalidate();
}

/// Initializes the kernel 
void kernel_init(void)
{
    mm_init();
    sched_init();
}   

extern volatile enum loader_state loader_state;


extern struct rq rq;

void mmc(void* args)
{
    mmc_init(MMC1);
    while (1);
}

void task_manager(void* args)
{
    while (1) {
        syscall_thread_sleep(1000000);
        ///print("Thread says => %s\n", text);
        struct list_node* it;
        list_iterate(it, &rq.thread_list) {
            struct thread* t = list_get_entry(it, struct thread, thread_node);

            u32 w_runtime = t->window_runtime;
            u32 w = rq.time.window;
            u32 percent = (100 * w_runtime) / w;
            u32 fraction = ((100 * w_runtime) / (w / 100)) - (percent * 100);

            u32 mem_kib = t->page_cnt * 4;

            print(" > NAME: %-16s  |  PID: %5d  |  CPU: %2d.%02d %%  |  Memory: %4d KiB\n", 
                t->name, t->pid, percent, fraction, mem_kib);
        }
        print("---------------------------------------------------\n");
    }
}

void main(void)
{
    print(BLUE "Cinnamon starting...\n" NORMAL);
    
    early_init();
    kernel_init();

    print("Adding threads\n");
    //create_process(task_manager, 500, "Task manager", "HELLO", SCHED_RT);
    create_process(mmc, 500, "MMC driver", NULL, SCHED_RT);

    print("Start the scheduler\n");
    sched_start();

    while (1);
}
