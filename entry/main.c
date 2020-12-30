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

#include <net/ip.h>
#include <net/arp.h>
#include <net/netbuf.h>
#include <net/net_rec.h>

#include <gfx/window.h>
#include <gfx/ttf.h>
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

static u8 buffer[1500];

struct netbuf buf;


i32 send(void* arg)
{
    u8 data[120];
    mem_set(data, 0xaa, 120);

    buf.buf = buffer;
    buf.buf_len = 1500;
    buf.header = buffer + 50;
    mem_set(buffer + 50, 0xCC, 50);
    buf.frame_len = 50;

    ipaddr_t src;
    ipaddr_t dest;

    str_to_ipaddr("192.168.0.55", &src);
    str_to_ipaddr("192.168.0.177", &dest);

    
    
    while (1) {
        print("Sending\n");
        mac_out(&buf, src, dest, 0x0800);
        syscall_thread_sleep(1000);
        gmac_send_raw(data, 120);
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

    // Sending random requests
    create_kthread(send, 800, "nic send", NULL, SCHED_RT);

    arp_init();
    net_rec_init();

    sched_start();
} 
