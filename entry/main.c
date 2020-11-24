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

    print_logo();
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
    print_task_init();
    mmc_init();
    dma_init();
    dma_receive_init();
    lcd_init();
}


void draw(struct rgb (*buffer)[800], struct rgb* rgb, u16 x, u16 y, u16 w, u16 h)
{
    for (u32 i = y; i < y + h; i++) {
        for (u32 j = x; j < x + w; j++) {
            buffer[i][j] = (struct rgb){ .b = rgb->b, .g = rgb->g, .r = rgb->r };
        }
    }
}

void draw_rgba(struct rgba (*buffer)[800], struct rgba* rgb, u16 x, u16 y, u16 w, u16 h)
{
    for (u32 i = y; i < y + h; i++) {
        for (u32 j = x; j < x + w; j++) {
            buffer[i][j] = (struct rgba){ .b = rgb->b, .g = rgb->g, .r = rgb->r, .a = rgb->a };
        }
    }
}

void del(u32 cyc)
{
    for (u32 i = 0; i < cyc; i++) {
        asm ("nop");
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

    sched_start();
} 
