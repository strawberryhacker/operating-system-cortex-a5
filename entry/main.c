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
#include <citrus/list.h>
#include <citrus/panic.h>
#include <citrus/elf.h>
#include <citrus/gpio.h>
#include <citrus/mmc.h>
#include <citrus/task_manager.h>
#include <citrus/disk.h>
#include <citrus/dma.h>
#include <gfx/engine.h>
#include <citrus/fpu.h>
#include <citrus/dma_receive.h>
#include <citrus/logo.h>
#include <citrus/lcd.h>
#include <citrus/fat.h>
#include <citrus/fs.h>
#include <gfx/font.h>
#include <citrus/error.h>

#include <app/led_strip.h>

#include <citrus/regmap.h>
#include <stdarg.h>
#include <stdalign.h>

/// Early initialization for the kernel
void early_init(void)
{
    apic_init();

    // The loader is in control of the kernel download serial line and 
    // should be initialized first in order to access the bootloader. 
    //loader_init();

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
    mmc_init();
    dma_init();
    dma_receive_init();
}

extern volatile struct rgb framebuffer[800*480];

u32 test_thread(void* args)
{
    syscall_thread_sleep(1000);
    print("Starting test thread\n");

    struct file_info* info = kmalloc(sizeof(struct file_info));
    struct file* dir = dir_open("/sda1");
    if (!dir) {
        print("Cannot open file\n");
        return 0;
    }

    file_header();
    while (1) {
        i8 err = dir_read(dir, info);
        if (err)
            break;
        file_print(info);
        get_next_valid_entry(dir->part, dir);
    };

    print("Opening file\n");
    struct file* file = 
        file_open("/sda2/wallpapers/wallpaper-1.data", FILE_ATTR_R);

    if (file == NULL) {
        print("Cannot open file\n");
        return 0;
    }

    u32 ret_cnt;
    
    u8* buffer = (u8 *)framebuffer;
    u8* b = kmalloc(3000);
    do {
        i8 err = file_read(file, b, 3000, &ret_cnt);
        if (err)
            break;
        for (u32 i = 0; i < ret_cnt; i += 3) {
            u32 reg = ((b[i] >> 2) << 12) | ((b[i + 1] >> 2) << 6) |
                ((b[i + 2] >> 2) << 0);
            *buffer++ = (b[i + 2] >> 2) | (((b[i + 1] >> 2) & 0b11) << 6);
            *buffer++ = (b[i + 1] >> 4) | (((b[i] >> 2) & 0b1111) << 4);
            *buffer++ = (b[i] >> 6) & 0b11;
        }
        dcache_clean();
    } while (ret_cnt == 3000);
    test();
    return 1;
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
    //task_manager_init();
    struct lcd_info lcd_info = {
        .height        = 480,
        .width         = 800,
        .framerate     = 60,
        .v_back_porch  = 21,
        .v_front_porch = 22,
        .v_pulse_width = 2,
        .h_back_porch  = 64,
        .h_front_porch = 64,
        .h_pulse_width = 128
    };

    lcd_init();
    lcd_on(&lcd_info);
    create_kernel_thread(test_thread, 1000, "filesystem", NULL, SCHED_RT);

    sched_start();
} 
