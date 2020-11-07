/// Copyright (C) strawberryhacker

#ifndef DMA_H
#define DMA_H

#include <citrus/types.h>

enum dma_type {
    DMA_TYPE_MEM_MEM,
    DMA_TYPE_PER_MEM,
    DMA_TYPE_MEM_PER
};

enum dma_burst {
    DMA_BURST_1,
    DMA_BURST_4,
    DMA_BURST_8,
    DMA_BURST_16
};

enum dma_chunk {
    DMA_CHUNK_1,
    DMA_CHUNK_2,
    DMA_CHUNK_4,
    DMA_CHUNK_8,
    DMA_CHUNK_16
};

enum dma_data {
    DMA_DATA_U8,
    DMA_DATA_U16,
    DMA_DATA_U32,
    DMA_DATA_U64,
};

enum dma_trigger {
    DMA_TRIGGER_HW,
    DMA_TRIGGER_SW
};

enum dma_am {
    DMA_AM_FIXED,
    DMA_AM_INC,
    DMA_AM_STRIDE,
    DMA_AM_FULL_STRIDE,
};

/// Controls the descriptor structure
enum dma_desc_type {
    DMA_DESC0,
    DMA_DESC1,
    DMA_DESC2,
    DMA_DESC3
};

struct dma_desc0 {
    struct dma_desc0* next;
    u32 ctrl;
    u32 tx_addr;
};

struct dma_desc1 {
    struct dma_desc1* next;
    u32 ctrl;
    u32 src_addr;
    u32 dest_addr;
};

struct dma_desc2 {
    struct dma_desc2* next;
    u32 ctrl;
    u32 src_addr;
    u32 dest_addr;
    u32 cfg;
};

struct dma_desc3 {
    struct dma_desc3* next;
    u32 ctrl;
    u32 src_addr;
    u32 dest_addr;
    u32 cfg;
    u32 block_ctrl;
    u32 stride;
    u32 src_stride;
    u32 dest_stride;
};

/// DMA channel
struct dma_channel {
    struct dma_reg* hw;
    u8 lock;
};

/// Transfer descriptor
struct dma_req {
    void* src_addr;
    void* dest_addr;

    // Channel configuration
    enum dma_type type;
    enum dma_burst burst;
    enum dma_chunk chunk;
    enum dma_data data;
    enum dma_trigger trigger;
    enum dma_am dest_am;
    enum dma_am src_am;

    u8 non_secure     : 1;
    u8 memset_enable  : 1;
    u8 src_interface  : 1;
    u8 dest_interface : 1;

    u32 ublock_len;
    u32 ublock_cnt;

    void (*transfer_done)(struct dma_req*);

    // ID of the DMA peripheral
    u8 id;
};

/// Defines the DMA interrupts
#define DMA_EOB               (1 << 0)
#define DMA_EOL               (1 << 1)
#define DMA_EOD               (1 << 2)
#define DMA_EOF               (1 << 3)
#define DMA_READ_BUS_ERROR    (1 << 4)
#define DMA_WRITE_BUS_ERROR   (1 << 5)
#define DMA_REQ_OVERLOW_ERROR (1 << 6)

#define DMA_ERROR (DMA_READ_BUS_ERROR | DMA_WRITE_BUS_ERROR | \
    DMA_REQ_OVERLOW_ERROR)

u32 dma_init(void* args);

void dma_test_init(void);

u8 dma_submit_request(struct dma_req* req);

#endif
