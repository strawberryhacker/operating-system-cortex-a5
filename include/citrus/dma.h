/// Copyright (C) strawberryhacker

#ifndef DMA_H
#define DMA_H

#include <citrus/types.h>

struct dma {

};

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

#define DMA_END_OF_BLOCK    (1 << 0)
#define DMA_END_OF_LIST     (1 << 1)
#define DMA_END_OF_DISABLE  (1 << 2)
#define DMA_END_OF_FLUSH    (1 << 3)
#define DMA_READ_BUS_ERROR  (1 << 4)
#define DMA_WRITE_BUS_ERROR (1 << 5)
#define DMA_OVERFLOW        (1 << 6)

void dma_init(void);

#endif
