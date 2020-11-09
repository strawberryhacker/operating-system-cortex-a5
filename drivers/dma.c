/// Copyright (C) strawberryhacker

#include <citrus/dma.h>
#include <citrus/panic.h>
#include <citrus/print.h>
#include <citrus/apic.h>
#include <citrus/clock.h>
#include <citrus/mm.h>
#include <citrus/thread.h>
#include <citrus/cache.h>
#include <citrus/syscall.h>
#include <citrus/atomic.h>
#include <regmap.h>

#define UART1_DMA_CH 37
#define DMA_CHANNELS 16

/// List of all the available DMA channels in the system
static struct dma_channel dma_channels[DMA_CHANNELS * 2];

/// Intialized the DMA hardware making it operational
static void dma_init_hardware(struct dma_reg* dma)
{
    
}

/// Common DMA interrupt handler
static void dma_common_interrupt(struct dma_reg* dma)
{

    print("DMA interrupt\n");
    
    // Get the first interrupting DMA channel
    u8 ch;
    u32 reg = dma->GIS;
    for (ch = 0; ch < 16; ch++) {
        if (reg & (1 << ch)) {
            break;
        }
    }

    print("Channel %d\n", ch);

    print("Status => %08b\n", dma->channel[ch].CIS);

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

/// Initializes both system DMAs
void dma_init(void)
{
    // Initialize the clocks
    clk_pck_enable(6);
    clk_pck_enable(7);

    // Initialize the APIC controler
    apic_add_handler(6, dma0_interrupt);
    apic_add_handler(7, dma1_interrupt);

    apic_enable(6);
    apic_enable(7);

    dma_init_hardware(DMA0);
    dma_init_hardware(DMA1);

    // Setup the DMA channels
    for (u8 i = 0; i < DMA_CHANNELS; i++) {
        dma_channels[i].hw = DMA0;
        dma_channels[i].ch = i;
        dma_channels[i].free = 1;
    }
    for (u8 i = 0; i < DMA_CHANNELS; i++) {
        dma_channels[i + DMA_CHANNELS].hw = DMA0;
        dma_channels[i + DMA_CHANNELS].ch = i;
        dma_channels[i + DMA_CHANNELS].free = 1;
    }
}

static void dma_fill_microblock_transfer(struct dma_reg* dma, u8 ch, 
    struct dma_req* req)
{
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

    // Clear linked list registers
    dma->channel[ch].CNDC = 0;
    dma->channel[ch].CDS_MSP = 0;
    dma->channel[ch].CSUS = 0;
    dma->channel[ch].CDUS = 0;

    // Enable end of block interrupt
    dma->channel[ch].CIE = DMA_EOB | DMA_ERROR;
    dma->GIE = (1 << ch);
    
    // Enable the channel
    dma->GE = (1 << ch);
}

/// Allocates a DMA channel
struct dma_channel* alloc_dma_channel(void)
{
    u32 flags = __atomic_enter();

    // Get a both free and un-allocated channel
    for (u32 i = 0; i < DMA_CHANNELS * 2; i++) {
        struct dma_channel* ch = &dma_channels[i];
        if (ch->free) {
            ch->free = 0;
            print("CHANNEL => %p\n", ch);
            __atomic_leave(flags);
            return ch;
        }
    }

    __atomic_leave(flags);
    return NULL;
}

/// Frees a DMA channel
void free_dma_channel(struct dma_channel* ch)
{
    // TODO stop any transfers on this channel
    u32 flags = __atomic_enter();
    ch->free = 1;
    __atomic_leave(flags);
}

u8 dma_submit_request(struct dma_req* req, struct dma_channel* ch)
{
    dma_fill_microblock_transfer(ch->hw, ch->ch, req);
    return 1;
}

void dma_flush_channel(struct dma_channel* ch)
{
    u32 ch_num = ch->ch;
    struct dma_reg* dma = ch->hw;

    // Flush the channel
    dma->GSWF = (1 << ch_num);

    // Wait for the FIFO flush to complete
    //while (!(dma->channel[ch_num].CIS & (1 << 3)));
}

void dma_stop(struct dma_channel* ch)
{
    ch->hw->GD = (1 << ch->ch);
}

u32 dma_get_microblock_size(struct dma_channel* ch)
{
    return ch->hw->channel[ch->ch].CUBC;
}
