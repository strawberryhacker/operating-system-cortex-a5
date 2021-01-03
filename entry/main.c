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
#include <net/netbuf.h>
#include <net/mac.h>
#include <net/arp.h>
#include <net/udp.h>
#include <net/dhcp.h>
#include <net/tftp.h>

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
    print_dma_init();
}

i32 udp_test(void* arg)
{
    udp_listen(50);

    while (1) {
        struct netbuf* buf = udp_rec(50);
        if (buf) {
            print("UDP with length %d - %*s\n", buf->frame_len, buf->frame_len, buf->ptr);
        }
    }
}

i32 tx(void* arg)
{
    do {
        syscall_thread_sleep(1000);

        // Allocate a netbuffer
        struct netbuf* buf = alloc_netbuf();
        if (buf == NULL) 
            panic("Cant alloc netbufg\n");

        u32 ip;
        i32 err = str_to_ipv4("192.168.5.177", &ip);
        if (err) 
            panic("Wrong IP\n");

        // Copy in the data
        mem_copy("this is called a UDP packet", buf->ptr, 27);
        buf->frame_len = 27;

        udp_send(buf, ip, 80, 0);

    } while (1);
    while (1);
}

i32 tftp_test(void* arg)
{
    syscall_thread_sleep(2000);

    struct tftp tftp;
    tftp_create(&tftp, "192.168.5.177");

    u8* file;
    u32 filesize;

    i32 err = tftp_download(&tftp, "doc/arm_basics.md", &file, &filesize);

    if (err)
        panic("Error");

    while (filesize--) {
        char c = *file++;
        if (c != '\r')
            print("%c", c);
    }

    while (1) {
        syscall_thread_sleep(500);
    }
}

/// Called by entry.s after low level initialization finishes
void main(void)
{
    // Initialize the kernel system
    early_init();
    kernel_init();
    driver_init();

    print("\n\nStarting networking\n");

    // Custom init
    gmac_init();
    mac_init();
    arp_init();
    udp_init();

    // ==================================================
    // Add the kernel threads / startup routines below 
    // ==================================================

    create_kthread(tx, 5000, "net tx", NULL, SCHED_RT);
    create_kthread(udp_test, 5000, "udp", NULL, SCHED_RT);
    create_kthread(tftp_test, 5000, "tftp", NULL, SCHED_RT);
    dhcp_init();

    sched_start();
} 
