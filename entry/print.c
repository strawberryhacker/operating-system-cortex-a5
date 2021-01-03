// Copyright (C) strawberryhacker 

#include <citrus/print.h>
#include <citrus/sprint.h>
#include <citrus/gpio.h>
#include <citrus/clock.h>
#include <citrus/interrupt.h>
#include <citrus/regmap.h>
#include <citrus/cache.h>
#include <citrus/dma.h>
#include <citrus/panic.h>
#include <citrus/atomic.h>
#include <citrus/mm.h>
#include <stddef.h>

static char buffer[1024];

void print_dep(const char* data, ...)
{
    va_list args;
    va_start(args, data);
    u32 cnt = vsprint(buffer, data, args);
    va_end(args);

    const u8* start = (const u8 *)buffer;
    while (cnt--) {
        while (!(UART1->SR & (1 << 1)));
        UART1->THR = *start;
        start++;
    }
}

static char buffer_task[1024];

void print_init(void)
{
    // Setup the pins
    gpio_set_func(&(struct gpio){ .hw = GPIOB, .pin = 3 }, GPIO_FUNC_A);
    gpio_set_func(&(struct gpio){ .hw = GPIOB, .pin = 4 }, GPIO_FUNC_A);

    clk_pck_enable(28);

    struct uart_reg* const uart = UART4;

    uart->MR = (4 << 9);
    uart->BRGR = 23;
    uart->CR = (1 << 6);
}

void print_task(const char* data, ...)
{
    va_list args;
    va_start(args, data);
    u32 cnt = vsprint(buffer_task, data, args);
    va_end(args);

    const u8* start = (const u8 *)buffer_task;
    while (cnt--) {
        while (!(UART4->SR & (1 << 1)));
        UART4->THR = *start;
        start++;
    }
}

const char test[] = "Hello this is a test\n";

#define PRINT_SIZE 10240

static u8 buf_a[PRINT_SIZE];
static u8 buf_b[PRINT_SIZE];

struct print_state {
    u8* curr_buf;
    u32 curr_len;

    u8* dma_buf;

    u8 dma_done;
};

static struct print_state state;

void print_dma_callback(struct dma_req* req);

// Allocate channel for all print traffic
static struct dma_channel* print_dma_ch;

// Initializes the print
void print_dma_init(void)
{
    // Allocate a new DMA channel
    print_dma_ch = alloc_dma_channel();
    if (print_dma_ch == NULL) 
        panic("Cant allocate DMA channel");

    state.curr_buf = buf_a;
    state.dma_buf = buf_b;

    state.curr_len = 0;
    state.dma_done = 1;
}


// Sends the current buffer on DMA
void flush_buffer(void)
{
    state.dma_done = 0;

    // Setup DMA
    struct dma_req req = {
        .burst = DMA_BURST_1,
        .chunk = DMA_CHUNK_1,
        .data = DMA_DATA_U8,
        .dest_addr = (void *)&UART1->THR,
        .dest_am = DMA_AM_FIXED,
        .dest_interface = 1,
        .id = 37,
        .memset_enable = 0,
        .non_secure = 0,
        .src_addr = va_to_pa((void *)state.curr_buf),
        .src_am = DMA_AM_INC,
        .src_interface = 0,
        .transfer_done = print_dma_callback,
        .trigger = DMA_TRIGGER_HW,
        .type = DMA_TYPE_MEM_PER,
        .ublock_cnt = 1,
        .ublock_len = state.curr_len
    };

    dcache_clean_range((u32)state.curr_buf, (u32)(state.curr_buf + 
        state.curr_len));
        
    // Swap the buffer
    u8* curr = state.curr_buf;
    state.curr_buf = state.dma_buf;
    state.dma_buf = curr;

    state.curr_len = 0;

    dma_submit_request(&req, print_dma_ch);
}

// Called in the interrupt when the transfer is done
void print_dma_callback(struct dma_req* req)
{
    state.dma_done = 1;

    // We have to check if the pending buffer has any data
    if (state.curr_len) {

        // Send curr on DMA
        flush_buffer();
    }
}

void print(const char* data, ...)
{
    va_list args;
    va_start(args, data);
    u32 cnt = vsprint((char *)state.curr_buf + state.curr_len, data, args);
    va_end(args);

    // Update the size
    u32 atomic = __atomic_enter();
    state.curr_len += cnt;
    __atomic_leave(atomic);

    // Check if the DMA is done. If the DMA is done we start the next reqest
    // right away
    if (state.dma_done) {

        // Send curr on DMA
        flush_buffer();

    } else {
        // The DMA is still waiting
    }
}

void print_dma_test(void)
{
    u32 len = sizeof(test);
    
    struct dma_channel* ch = alloc_dma_channel();

    if (ch == NULL) 
        panic("Cant allocate DMA channel");
    
    // We need the PER ID of UART4
    u32 id = 37;

    struct dma_req req = {
        .burst = DMA_BURST_1,
        .chunk = DMA_CHUNK_1,
        .data = DMA_DATA_U8,
        .dest_addr = (void *)&UART1->THR,
        .dest_am = DMA_AM_FIXED,
        .dest_interface = 1,
        .id = id,
        .memset_enable = 0,
        .non_secure = 0,
        .src_addr = va_to_pa((void *)test),
        .src_am = DMA_AM_INC,
        .src_interface = 0,
        .transfer_done = print_dma_callback,
        .trigger = DMA_TRIGGER_HW,
        .type = DMA_TYPE_MEM_PER,
        .ublock_cnt = 1,
        .ublock_len = len
    };

    dcache_clean();
    dma_submit_request(&req, ch);

}
