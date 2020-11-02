/// Copyright (C) strawberryhacker 

#include <citrus/types.h>
#include <citrus/sprint.h>
#include <citrus/apic.h>
#include <citrus/print.h>
#include <citrus/buddy_alloc.h>
#include <citrus/cache.h>
#include <citrus/mm.h>
#include <citrus/benchmark.h>
#include <citrus/thread.h>
#include <citrus/sched.h>
#include <citrus/page_alloc.h>
#include <citrus/pt_entry.h>
#include <citrus/syscall.h>
#include <citrus/interrupt.h>
#include <citrus/atomic.h>
#include <citrus/kmalloc.h>
#include <citrus/loader.h>
#include <citrus/panic.h>
#include <citrus/elf.h>
#include <citrus/gpio.h>
#include <citrus/mmc.h>
#include <citrus/task_manager.h>
#include <citrus/disk.h>
#include <citrus/dma.h>

#include <app/led_strip.h>

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
    irq_enable();
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
    //mmc_init();
    //dma_init();
}

/// Called by entry.s after low level initialization finishes
void main(void)
{
    // Initialize the kernel system
    early_init();
    kernel_init();
    driver_init();

    // ==================================================
    // Add the kernel threads / startup routines below 
    // ==================================================
    task_manager_init();
    led_strip_init();

    sched_start();
} 
