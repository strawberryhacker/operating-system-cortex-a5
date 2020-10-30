/// Copyright (C) strawberryhacker

#include <cinnamon/dma.h>
#include <cinnamon/panic.h>
#include <cinnamon/print.h>
#include <cinnamon/apic.h>
#include <cinnamon/clock.h>
#include <regmap.h>

static inline void dma_channel_irq_en(struct dma_reg* dma, u8 channel)
{
    dma->GIE = (1 << channel);
}

static inline void dma_channel_irq_dis(struct dma_reg* dma, u8 channel)
{
    dma->GID = (1 << channel);
}

static inline u32 dma_get_status(struct dma_reg* dma, u8 channel)
{
    return dma->GIS;
}

static inline void dma_channel_enable(struct dma_reg* dma, u8 channel)
{
    dma->GE = (1 << channel);
}

static inline void dma_channel_disable(struct dma_reg* dma, u8 channel)
{
    dma->GD = (1 << channel);
}

/// Intialized the DMA hardware making it operational
static void dma_init_hardware(struct dma_reg* dma)
{

}

/// Common DMA interrupt handler
static void dma_common_interrupt(struct dma_reg* reg)
{

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
    print("DMA starting reg => %p\n", &DMA0->channel[9].CDS_MSP);

    // Initialize the clocks

    // Initialize the APIC controler
    apic_add_handler(6, dma0_interrupt);
    apic_add_handler(7, dma1_interrupt);

    apic_enable(6);
    apic_enable(7);

    dma_init_hardware(DMA0);
    dma_init_hardware(DMA1);
}
