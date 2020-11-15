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

u32 test_thread(void* args)
{
    syscall_thread_sleep(500);
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
        get_next_valid_entry(dir);
    };

    print("Opening file\n");
    struct file* file = 
        file_open("/sda2/wallpapers/wallpaper-5.data", FILE_R);

    if (file == NULL) {
        print("Cannot open file\n");
        return 0;
    }

    u32 ret_cnt;
    
    u32* buffer = lcd_get_framebuffer(0);
    u8* b = kmalloc(3000);
    u8 l = 0;
    do {
        i8 err = file_read(file, b, 3000, &ret_cnt);
        if (err)
            break;
        for (u32 i = 0; i < ret_cnt; i += 3) {
            u32 reg = get_color(b[i], b[i + 1], b[i + 2], 0xFF);
            *buffer++ = reg;
        }
    } while (ret_cnt == 3000);
    dcache_clean();
    return 1;
}

extern volatile u16 x;
extern volatile u16 y;
u32 test_threada(void* args)
{
    syscall_thread_sleep(2000);
    print("Starting test thread\n");

    struct file_info* info = kmalloc(sizeof(struct file_info));
    struct file* dir = dir_open("/sda2");
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
        get_next_valid_entry(dir);
    };

    print("Opening file\n");
    struct file* file = 
        file_open("/sda2/mouse-icon1.data", FILE_R);

    if (file == NULL) {
        print("Cannot open file\n");
        return 0;
    }

    u32 ret_cnt;
    
    u32 (*buffer)[800] = (u32 (*)[800])lcd_get_framebuffer(2);
    u8* b = kmalloc(10000);
    u8 l = 0;
    do {
        i8 err = file_read(file, b, 10000, &ret_cnt);
        if (err)
            break;
    } while (ret_cnt == 10000);
    print("Ret count => %d\n", ret_cnt);

    u16 prev_x = 0;
    u16 prev_y = 0;
    u16 curr_x;
    u16 curr_y;
    while (1) {

        u32 c = get_color(0, 0, 0, 0);
        set_color(lcd_get_framebuffer(2), prev_x, prev_y, 32, 48, c);
        u8* data = (u8 *)b;

        prev_y = curr_y;
        prev_x = curr_x;
        curr_x = x;
        curr_y = y;
        
        for (u32 i = 0; i < 24; i++) {
            for (u32 j = 0; j < 16; j++) {
                u8 fill = (data[1] == 0xFF) ? 0xFF : 0;
                u32 reg = get_color(data[0], data[0], data[0], fill);
                data += 2;
                buffer[curr_y + i][curr_x + j] = reg;
            }
        }
        dcache_clean();
        syscall_thread_sleep(1);
    }
    return 1;
}

u32 disp(void* arg)
{
    u32* buffer = lcd_get_framebuffer(1);
    u32 reg = get_color(15, 15, 15, 0xFF);
    set_color(buffer, 200, 50, 500, 300, reg);
    reg = get_color(15, 15, 15, 0x0);
    set_color(buffer, 250, 100, 100, 100, reg);
    
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
    

    lcd_init();
    create_kthread(test_thread, 1000, "filesystem", NULL, SCHED_RT);
    create_kthread(test_threada, 1000, "filesystem", NULL, SCHED_RT);
    create_kthread(disp, 1000, "disp", NULL, SCHED_RT);

    sched_start();
} 
