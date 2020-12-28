/// Copyright (C) strawberryhacker 

#include <citrus/types.h>
#include <citrus/apic.h>
#include <citrus/print.h>
#include <citrus/thread.h>
#include <citrus/syscall.h>
#include <citrus/kmalloc.h>
#include <citrus/panic.h>
#include <citrus/gpio.h>
#include <citrus/mmc.h>
#include <citrus/interrupt.h>
#include <citrus/task_manager.h>
#include <citrus/disk.h>
#include <citrus/dma.h>
#include <citrus/fpu.h>
#include <citrus/dma_receive.h>
#include <citrus/sched.h>
#include <citrus/cache.h>
#include <citrus/logo.h>
#include <citrus/lcd.h>
#include <citrus/fat.h>
#include <citrus/fs.h>
#include <citrus/error.h>
#include <citrus/regmap.h>
#include <citrus/pid.h>
#include <citrus/mem.h>
#include <citrus/gmac.h>

#include <gfx/window.h>
#include <gfx/ttf.h>
#include <gfx/font.h>

#include <stdarg.h>
#include <stdalign.h>

/// Early initialization for the kernel
void early_init(void)
{
    apic_init();

    // Enable interrupt now to support reboot
    irq_enable();
    async_abort_enable();

    // Enable the L1 cache
    icache_enable();
    dcache_enable();

    // Enable access to FPU co-processors
    fpu_init();
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
    print_init();
    dma_init();
    dma_receive_init();
}

// Test code for the network stack
i32 rec(void* arg)
{
    while (1) {
        gmac_receive();
    }
}

i32 send(void* arg)
{
    u8 data[120];
    mem_set(data, 0xaa, 120);
    while (1) {
        syscall_thread_sleep(1000);
        nic_send_raw(data, 120);
    }
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

    print("\n");
    gmac_init();

    create_kthread(rec, 800, "nic rec", NULL, SCHED_RT);
    create_kthread(send, 800, "nic send", NULL, SCHED_RT);

    sched_start();
} 
