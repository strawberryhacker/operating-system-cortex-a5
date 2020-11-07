/// Copyright (C) strawberryhacker

#include <citrus/dma_receive.h>
#include <citrus/apic.h>
#include <citrus/thread.h>
#include <citrus/cache.h>
#include <citrus/dma.h>
#include <citrus/mm.h>
#include <citrus/panic.h>
#include <regmap.h>
#include <stdalign.h>

#define DMA_BUFFER_SIZE 4096

/// Must be aligned with a cache line in order to invalidate the serial buffer
/// the right way
static volatile u8 alignas(32) dma_buffer[DMA_BUFFER_SIZE];

static struct dma_req dma_req;
static struct dma_channel* dma_ch;

void uart_interrupt(void)
{
    // Check if there is a timeout
    if (UART4->SR & (1 << 8)) {
        print("Timeout occured\n");

        // Flush the rest of the DMA
        print("Flushing the channel\n");
        //dma_flush_channel(dma_ch);
        dma_stop(dma_ch);

        print("CUBS => %d\n", dma_get_microblock_size(dma_ch));
        print("Received %d\n", DMA_BUFFER_SIZE - dma_get_microblock_size(dma_ch));

        print("Invalidating the cache\n");
        dcache_invalidate_range((u32)dma_buffer, 
            (u32)(dma_buffer + DMA_BUFFER_SIZE));

        print("Resp: %20s\n", (char *)dma_buffer);

        while (1);
    }
}

static void arm_dma(void)
{
    dma_submit_request(&dma_req, dma_ch);
}

void dma_receive_init(void)
{
    // Setup the DMA request so it is fast to reconfigure the channel
    dma_req = (struct dma_req){
        .burst          = DMA_BURST_16,
        .chunk          = DMA_CHUNK_1,
        .data           = DMA_DATA_U8,
        .dest_am        = DMA_AM_INC,
        .src_am         = DMA_AM_FIXED,
        .non_secure     = 0,
        .memset_enable  = 0,
        .dest_interface = 0,
        .src_interface  = 1,
        .type           = DMA_TYPE_PER_MEM,
        .trigger        = DMA_TRIGGER_HW,
        .ublock_cnt     = 1,
        .ublock_len     = DMA_BUFFER_SIZE,
        .id             = 44,
        .src_addr       = (void *)&UART4->RHR,
        .dest_addr      = va_to_pa((void *)dma_buffer)
    };

    // Modify the serial to only throw an exception at timeout
    UART4->IDR = 0xFFFF;
    UART4->IER = (1 << 8);

    // IDLE condition is 5 characters
    UART4->RTOR = 8 * 5;
    UART4->CR = (1 << 11);

    // Add the loader interrupt handler
    apic_add_handler(28, uart_interrupt);
    apic_set_priority(28, 2);
    apic_enable(28);

    dma_ch = alloc_dma_channel();

    if (dma_ch == NULL) {
        panic("Error\n");
    }

    arm_dma();
}

