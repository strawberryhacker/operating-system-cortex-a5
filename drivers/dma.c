/// Copyright (C) strawberryhacker

#include <citrus/dma.h>
#include <citrus/panic.h>
#include <citrus/print.h>
#include <citrus/apic.h>
#include <citrus/clock.h>
#include <citrus/mm.h>
#include <regmap.h>

#define UART1_DMA_CH 37

/// Intialized the DMA hardware making it operational
static void dma_init_hardware(struct dma_reg* dma)
{

}

/// Common DMA interrupt handler
static void dma_common_interrupt(struct dma_reg* reg)
{
    print("DMA interrupt\n");

    while (1);
}

/// Main DMA0 interrupt handler
static void dma0_interrupt(void)
{
    dma_common_interrupt(DMA0);
}

/// Main DMA1 interrupt handler
static void dma1_interrupt(void)
{
    dma_common_interrupt(DMA1);
}

volatile char buffer[] = "I love DMA";

/// Initializes both system DMAs
void dma_init(void)
{
    print("DMA starting reg => %p\n", &DMA0->channel[9].CDS_MSP);

    // Initialize the clocks

    // Initialize the APIC controler
    apic_add_handler(6, dma0_interrupt);
    apic_add_handler(7, dma1_interrupt);

    apic_enable(6);
    apic_enable(7);

    dma_init_hardware(DMA0);
    dma_init_hardware(DMA1);

    struct dma_req req = {
        .burst          = DMA_BURST_1,
        .chunk          = DMA_CHUNK_1,
        .data           = DMA_DATA_U8,
        .dest_am        = DMA_AM_FIXED,
        .src_am         = DMA_AM_INC,
        .non_secure     = 1,
        .memset_enable  = 0,
        .id             = UART1_DMA_CH,
        .dest_interface = 0,
        .src_interface  = 0,
        .type           = DMA_TYPE_MEM_PER,
        .trigger        = DMA_TRIGGER_HW,
        .ublock_cnt     = 1,
        .ublock_len     = 11,
        .src_addr       = va_to_pa((void *)buffer),
        .dest_addr      = (void *)&UART1->THR
    };
    dma_submit_request(&req);
}

/// Returns a free DMA channel number and the corresponding hardware
static inline u8 dma_get_ch_hw_pair(struct dma_reg** dma, u8* ch)
{
    for (u8 i = 0; i < 16; i++) {
        if ((DMA0->GS & (1 << i)) == 0) {
            *ch = i;
            *dma = DMA0;
            return 1;
        }
    }
    for (u8 i = 0; i < 16; i++) {
        if ((DMA1->GS & (1 << i)) == 0) {
            *ch = i;
            *dma = DMA1;
            return 1;
        }
    }
    return 0;
}

static void dma_fill_microblock_transfer(struct dma_reg* dma, u8 ch, 
    struct dma_req* req)
{
    if (dma == DMA0) {
        print("Ready for DMA0 transfer channel %d\n", ch);
    } else if (dma == DMA1) {
        print("Ready for DMA1 transfer channel %d\n", ch);
    }
    // Clear interrupt pending status
    (void)dma->channel[ch].CIS;

    dma->channel[ch].CSA = (u32)req->src_addr;
    dma->channel[ch].CDA = (u32)req->dest_addr;

    // Set the microblock and block length
    dma->channel[ch].CUBC = req->ublock_len & 0xFFFFFF;

    assert(req->ublock_cnt);
    dma->channel[ch].CBC = req->ublock_cnt - 1;

    // Fill in the channel control register
    u32 reg = ((req->id & 0x7F) << 24) | (req->burst << 1) | 
        (req->non_secure << 5) | (req->trigger << 6) | 
        (req->memset_enable << 7) | (req->chunk << 8) | (req->data << 11) | 
        (req->src_interface << 13) | (req->dest_interface << 14) | 
        (req->src_am << 16) | (req->dest_am << 18);

    if (req->type != DMA_TYPE_MEM_MEM) {
        reg |= (1 << 0);

        // D-sync
        if (req->type == DMA_TYPE_MEM_PER) {
            reg |= (1 << 4);
        }
    }

    // Write the channel configuration
    dma->channel[ch].CC = reg;

    dma->channel[ch].CSUS = 0;
    dma->channel[ch].CDS_MSP = 0;

    // Clear linked list registers
    dma->channel[ch].CNDC = 0;
    dma->channel[ch].CDS_MSP = 0;
    dma->channel[ch].CSUS = 0;
    dma->channel[ch].CDUS = 0;

    // Enable end of block interrupt
    dma->channel[ch].CIE = DMA_EOB | 0xFF;
    dma->GIE = (1 << ch);
    
    // Enable the channel
    dma->GE = (1 << ch);
    dma->GSWR = (1 << ch);
}

u8 dma_submit_request(struct dma_req* req)
{
    u8 ch = 0;
    struct dma_reg* dma;

    if (dma_get_ch_hw_pair(&dma, &ch) == 0) {
        print("Error\n");
        return 0;
    }

    print("Free channel %d\n", ch);

    dma_fill_microblock_transfer(dma, ch, req);
    return 1;
}
