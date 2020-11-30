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

enum dma_dir {
    DMA_DIR_PER_TO_MEM,
    DMA_DIR_MEM_TO_PER
};

/// Controls the descriptor structure
enum dma_desc_type {
    DMA_DESC_TYPE_0,
    DMA_DESC_TYPE_1,
    DMA_DESC_TYPE_2,
    DMA_DESC_TYPE_3
};

// Descriptor structures. These are linked with the first member and the next
// descriptor type is set with the ublock_ctrl member
struct dma_desc0 {
    void* next;
    u32 ublock_ctrl;
    u32 tx_addr;
};

struct dma_desc1 {
    void* next;
    u32 ublock_ctrl;
    u32 src_addr;
    u32 dest_addr;
};

struct dma_desc2 {
    void* next;
    u32 ublock_ctrl;
    u32 src_addr;
    u32 dest_addr;
    u32 config;
};

struct dma_desc3 {
    void* next;
    u32 ublock_ctrl;
    u32 src_addr;
    u32 dest_addr;
    u32 config;
    u32 block_ctrl;
    i32 data_stride;
    i32 src_stride;
    i32 dest_stride;
};

/// DMA channel
struct dma_channel {
    struct dma_reg* hw;
    u8 ch;
    u8 free;
    u8 lock;

    void (*done)(void*);
    void* arg;
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

// PERID when no peripheral is used
#define NO_PERID 127

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

void dma_init(void);

struct dma_channel* alloc_dma_channel(void);

void free_dma_channel(struct dma_channel* ch);

u8 dma_submit_request(struct dma_req* req, struct dma_channel* ch);

void dma_flush_channel(struct dma_channel* ch);

void dma_stop(struct dma_channel* ch);

u32 dma_get_microblock_size(struct dma_channel* ch);

// Returns the micro-block control member used in linked list master transfer
static inline u32 dma_get_ublock_ctrl(enum dma_desc_type next_type, 
    u32 ublock_len, u32 next_desc_en, u32 src_update, u32 dest_update)
{
    return (next_type << 27) | (next_desc_en << 26) | (src_update << 25) |
        (dest_update << 24) | (ublock_len & 0xFFFFFF);
}

// Channel configuration information
struct dma_ch_cfg_info {
    u8 perid;

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
};

u32 dma_get_cfg_reg(struct dma_ch_cfg_info* info);

struct dma_master {
    void* first_addr;
    enum dma_desc_type first_type;
    u8 src_update;
    u8 dest_update;
};

void dma_start_master_transfer(void* first_desc, enum dma_desc_type type,
    u8 src_update, u8 dest_update, struct dma_channel* ch);

// NEW
i32 get_dma_channel(u8* channel);

//------------------------------------------------------------------------------

// API for DMA master transfer

struct dma_mem_mem_info {
    enum dma_burst burst;
    enum dma_data data;

    enum dma_am src_am;
    enum dma_am dest_am;

    u8 secure : 1;
    u8 memset: 1;
    u8 src_iface : 1;
    u8 dest_iface : 1;
};

struct dma_per_mem_info {
    enum dma_burst burst;
    enum dma_chunk chunk;
    enum dma_data data;
    enum dma_dir dir;

    enum dma_am src_am;
    enum dma_am dest_am;

    u8 secure : 1;
    u8 src_iface : 1;
    u8 dest_iface : 1;
    u8 soft_req : 1;

    u8 pid;
};

static inline u32 dma_get_config_mem_mem(struct dma_mem_mem_info* info)
{
    u32 reg = 0;
    reg |= info->burst << 1;
    reg |= info->data << 11;
    reg |= info->src_am << 16;
    reg |= info->dest_am << 18;
    reg |= (info->secure ^ 1) << 5;
    reg |= info->memset << 7;
    reg |= info->src_iface << 13;
    reg |= info->dest_iface << 14;
    reg |= 0b1111111 << 24;
    return reg;
}

static inline u32 dma_get_config_per_mem(struct dma_per_mem_info* info)
{
    u32 reg = 0;
    reg |= 1 << 1;
    reg |= info->burst << 1;
    reg |= info->chunk << 8;
    reg |= info->data << 11;
    reg |= info->src_am << 16;
    reg |= info->dest_am << 18;
    reg |= (info->secure ^ 1) << 5;
    reg |= info->src_iface << 13;
    reg |= info->dest_iface << 14;
    reg |= info->soft_req << 6;
    reg |= info->dir << 4;
    reg |= info->pid << 24;
    return reg;
}

static inline u32 dma_get_data_stride(u16 src_stride, u16 dest_stride)
{
    return (u32)((dest_stride << 16) | (src_stride << 0));
}

// Main DMA request block in case of master transfer
struct dma_master_req { 
    void* desc;               // First descriptor
    enum dma_desc_type type;  // First descriptor type

    u8 src_update : 1;        // Update source parameter when fetched
    u8 dest_update : 1;       // Update destination parameter when fetched
    u8 desc_fetch : 1;        // Set to 0 if desc count is 1

    void* private;            // Private data to be used in callback

    // DMA complete callback
    void (*done)(struct dma_master_req* req);
};

void dma_submit_master_req(struct dma_master_req* req, u8 channel);

#endif
