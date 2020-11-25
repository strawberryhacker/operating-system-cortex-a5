// Copyright (C) strawberryhacker

#include <citrus/dma_receive.h>
#include <citrus/apic.h>
#include <citrus/thread.h>
#include <citrus/cache.h>
#include <citrus/dma.h>
#include <citrus/mm.h>
#include <citrus/panic.h>
#include <citrus/crc.h>
#include <citrus/elf.h>
#include <citrus/page_alloc.h>
#include <citrus/mem.h>
#include <citrus/regmap.h>

#include <stdalign.h>

#define DMA_BUFFER_SIZE (4096 + 32 * 10)
#define CRC_POLY 0x45
#define PACKET_TAG 0xCA

#define CMD_DATA  0x00
#define CMD_SIZE  0x01
#define CMD_RESET 0x02
#define CMD_KILL  0x03

#define PACKET_ERROR 0x00 
#define PACKET_OK    0x01

static void uart4_interrupt(void);
static u8 process_packet(const u8* data, u32 size);
static u8 handle_packet(const u8* data, u32 size, u8 cmd);

// Packet header
struct packet_header {
    u8 tag;
    u8 crc;
    u8 cmd;
    u8 header_size;
    u32 data_size;
};

// Intermediate buffer for the DMA
static volatile u8 alignas(32) dma_buffer[DMA_BUFFER_SIZE];

// Allocate a DMA channel
static struct dma_channel* channel;

// Pre-allocate the DMA request
static struct dma_req req;

void flex_print_init(void)
{
    struct flexcom_reg* const hw = FLEX4;
    hw->U_CR = (1 << 3) | (1 << 2) | (1 << 8);
    hw->FLEX_MR = 1;
    hw->U_MR = (3 << 6) | (4 << 9) | (0b11 << 20);

    // Interrupt
    hw->U_BRGR = 5 | (5 << 16);

    hw->U_RTOR = 0;
    hw->U_TTGR = 0;

    hw->U_CR = (1 << 4) | (1 << 6);

    FLEX4->U_RTOR = 80;
    FLEX4->U_CR = (1 << 11);
    FLEX4->U_IER = (1 << 8);
}

void dma_receive_init(void)
{
    // Configure the DMA channel
    req = (struct dma_req) {
        .burst          = DMA_BURST_16,
        .chunk          = DMA_CHUNK_1,
        .data           = DMA_DATA_U8,
        .dest_addr      = va_to_pa((void *)dma_buffer),
        .dest_am        = DMA_AM_INC,
        .dest_interface = 0,
        .src_addr       = (void *)&FLEX4->U_RHR,
        .src_am         = DMA_AM_FIXED,
        .src_interface  = 1,
        .memset_enable  = 0,
        .non_secure     = 0,
        .transfer_done  = NULL,
        .trigger        = DMA_TRIGGER_HW,
        .type           = DMA_TYPE_PER_MEM,
        .id             = 20,
        .ublock_cnt     = 1,
        .ublock_len     = DMA_BUFFER_SIZE
    };

    // Allocate a DMA channel. This will never be freed
    channel = alloc_dma_channel();
    if (channel == NULL) {
        panic("Not enough channels");
    }
    flex_print_init();

    apic_add_handler(23, uart4_interrupt);
    apic_enable(23);

    dma_submit_request(&req, channel);
}

// Sends a response back to the computer
static void send_response(u8 resp)
{
    while (!(FLEX4->U_SR & (1 << 1)));
    FLEX4->U_THR = resp;
}

// Timeout interrupt for the UART4 channel
static void uart4_interrupt(void)
{   
    if (FLEX4->U_SR & (1 << 8)) {
        FLEX4->U_CR = (1 << 11);
        dma_flush_channel(channel);
        dma_stop(channel);
        u32 size = DMA_BUFFER_SIZE - dma_get_microblock_size(channel);

        // Since the DMA has trasferred to physical memory we have to invalidate
        // the memory
        dcache_invalidate_range((u32)dma_buffer, (u32)(dma_buffer + DMA_BUFFER_SIZE));      

        // Do somethong with the buffer
        u32 status = process_packet((u8 *)dma_buffer, size);
        dma_submit_request(&req, channel);

        // Send the status. This has to be done after the DMA is re-armed
        if (status) {
            send_response(PACKET_OK);
        } else {
            send_response(PACKET_ERROR);
        }
    }
}

static u8 process_packet(const u8* data, u32 size)
{
    // The data should have a packet header
    struct packet_header* header = (struct packet_header *)data;

    // Check if the tag matches
    if (header->tag != PACKET_TAG) {
        return 0;
    }

    u32 header_data_size = read_le32(&header->data_size);
    if (header_data_size + header->header_size != size) {
        print("%d + %d = %d\n", header_data_size, header->header_size, size);
        print("Packet size error\n");
        return 0;
    }

    // Verify the CRC
    u8 crc = crc_calculate(data + header->header_size, 
        header_data_size, CRC_POLY);
    
    if (crc != header->crc) {
        print("CRC mismatch\n");
        return 0;
    }
    
    return handle_packet(data + header->header_size, header_data_size, 
        header->cmd);
}


static volatile struct page* elf_page_buffer = NULL;
static volatile u8* elf_buffer = NULL;
static volatile u8* write_ptr = NULL;
static volatile u32 elf_size = 0;

static void clear_elf_buffers(void)
{
    if (elf_page_buffer != NULL) {
        //free_pages((struct page *)elf_page_buffer);
    }
    elf_buffer = NULL;
    write_ptr = NULL;
}

static void alloc_elf_buffers(u32 size)
{
    clear_elf_buffers();

    u32 order = bytes_to_order(size);
    elf_page_buffer = alloc_pages(order);

    if (elf_page_buffer != NULL) {
        elf_buffer = page_to_va((struct page *)elf_page_buffer);
        write_ptr = elf_buffer;
        elf_size = size;
    }
}

volatile u16 x = 0;
volatile u16 y = 0;
volatile u32 count = 0;

static u8 handle_packet(const u8* data, u32 size, u8 cmd)
{
    if (cmd == CMD_SIZE) {
        if (size != 4) {
            panic("Size packet is not 32-bytes");
            return 0;
        }
        u32 s = read_le32(data);
        alloc_elf_buffers(s);

    } else if (cmd == CMD_DATA) {
        for (u32 i = 0; i < size; i++) {
            *write_ptr++ = *data++;
        }

        if (size != 4096) {
            // Last or zero-length packet
            elf_init((u8 *)elf_buffer, elf_size);
            clear_elf_buffers();
        }
    } else if (cmd == CMD_RESET) {
        // Flush the serial buffer and go to the bootloader
        while (!(UART1->SR & (1 << 9)));
        RST->CR = 0xA5000000 | 1;
        while (1);
    } else if (cmd == 11) {
        x = read_le16(data);
        y = read_le16(data + 2);
        if (x >= (800 - 17)) {
            x = (800 - 17);
        }
        if (y >= (480 - 25)) {
            y = (480 - 25);
        }
    } else if (cmd == CMD_KILL) {
        if (size != 4) {
            print("Error with kill command\n");
        } else {
            u32 pid = read_le32(data);
            print("Killing thread with PID %d\n", pid);
        }
    }

    return 1;
}
