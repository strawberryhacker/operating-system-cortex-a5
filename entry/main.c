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
#include <cinnamon/task_manager.h>
#include <cinnamon/disk.h>

#include <regmap.h>
#include <stdarg.h>
#include <stdalign.h>

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

    // Enable the L1 cache
    icache_enable();
    dcache_enable();
}

/// Initializes the kernel 
void kernel_init(void)
{
    mm_init();
    sched_init();
    disk_init();
}

/// This will handle driver initialization
void driver_init(void)
{
    mmc_init();
}

extern struct rq rq;

/// Called by entry.s after low level initialization finishes
void main(void)
{
    // Initialize the kernel system
    early_init();
    kernel_init();
    //driver_init();

    // ==================================================
    // Add the kernel threads / startup routines below 
    // ==================================================
    task_manager_init();

    struct list_node* node;
    list_iterate(node, &rq.thread_list) {
        struct thread* t = list_get_entry(node, struct thread, thread_node);

        print("Thread > %s with mm %p\n", t->name, t->mm);
    }

    sched_start();
} 