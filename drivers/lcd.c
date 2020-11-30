// Copyright (C) strawberryhacker

#include <citrus/lcd.h>
#include <citrus/print.h>
#include <citrus/interrupt.h>
#include <citrus/clock.h>
#include <citrus/gpio.h>
#include <citrus/panic.h>
#include <citrus/cache.h>
#include <citrus/mm.h>
#include <citrus/mem.h>
#include <citrus/align.h>
#include <citrus/regmap.h>
#include <citrus/boot_alloc.h>

#include <stddef.h>
#include <stdalign.h>

struct lcd_dma_desc {
    u32 addr;
    u32 ctrl;
    u32 next;
    u32 padding;
};

// Private framebuffer structure in the system
struct lcd_layer {
    
    void* buffer[2];
    struct lcd_dma_desc* buffer_dma[2];

    // Physical address of the DMA descriptor and the framebuffer
    u32 buffer_pa[2];
    u32 buffer_dma_pa[2];

    u8 active_index : 1;

    // Will allways contain the right information of the current framebuffer
    struct screenbuffer info;
    
    // Register information
    struct lcd_ctrl* ctrl;
    struct lcd_dma* dma;
    volatile u32* cfg;
};

// One DMA per layer per buffer. This is allocated seperatly because of the
// 32-bit allignment which is required
struct lcd_dma_desc alignas(32) lcd_dma_descriptors[LAYERS * 2];

// Allocate the layers in use by the LCD controller
struct lcd_layer lcd_layers[LAYERS];

// Set to one when the LCD buffers are ready to use
static u8 lcd_buffers_ready = 0;

void lcd_layers_alloc(void)
{
    u32 pixels = SCREEN_X * SCREEN_Y;

    // Allocate aligned 1MB in order to manipulate the page tables making the 
    // LCD buffers non-cacheable
    lcd_layers[0].buffer[0] = boot_alloc(pixels * sizeof(struct rgb),  0xFFFFF);
    lcd_layers[0].buffer[1] = boot_alloc(pixels * sizeof(struct rgb),  32);
    lcd_layers[1].buffer[0] = boot_alloc(pixels * sizeof(struct rgba),  32);
    lcd_layers[1].buffer[1] = boot_alloc(pixels * sizeof(struct rgba),  32);
    lcd_layers[2].buffer[0] = boot_alloc(pixels * sizeof(struct rgba),  32);
    lcd_layers[2].buffer[1] = boot_alloc(pixels * sizeof(struct rgba),  32);

    // We should probably set the size at this point indicating how much was
    // allocated
    struct lcd_layer* layer = &lcd_layers[0];
    for (u32 i = 0; i < LAYERS; i++) {
        layer->info.h = SCREEN_Y;
        layer->info.w = SCREEN_X;
        
        if (i == 0)
            layer->info.bpp = 3; // Layer 1 uses no alpha channel
        else
            layer->info.bpp = 4;
        
        layer++;
    }

    // Total size of memory
    u32 size = ((u32)lcd_layers[2].buffer[1] + pixels * sizeof(struct rgba)) - 
        (u32)lcd_layers[0].buffer[0];

    u32 pages = align_up(size, 0xFFFFF) >> 20;

    // Mark the 1M pages as non-cacheable. ASID matching is not nessecary 
    // bacause they're in kernel space
    struct ste_attr attr = {
        .access = STE_ACCESS_FULL_ACC,
        .mem = STE_MEM_NON_CACHE,
        .domain = 15,
        .nG = 0,
        .xn = 0
    };

    u32* va = lcd_layers[0].buffer[0];
    for (u32 i = 0; i < pages; i++) {
        mm_change_kernel_pt_attr(va, &attr);
        va = (u32 *)((u8 *)va + 0x100000);
    }

    lcd_buffers_ready = 1;
}

static void lcd_pin_init(void)
{
    // LCD vsync
    gpio_set_func(&(struct gpio){ .hw = GPIOC, .pin = 5 }, GPIO_FUNC_A);

    // LCD hsync
    gpio_set_func(&(struct gpio){ .hw = GPIOC, .pin = 6 }, GPIO_FUNC_A);

    // LCD PCK 
    gpio_set_func(&(struct gpio){ .hw = GPIOC, .pin = 7 }, GPIO_FUNC_A);

    // LCD enable
    gpio_set_func(&(struct gpio){ .hw = GPIOC, .pin = 8 }, GPIO_FUNC_A);

    // LCD PWM
    gpio_set_func(&(struct gpio){ .hw = GPIOC, .pin = 3 }, GPIO_FUNC_A);

    // LCD display
    gpio_set_func(&(struct gpio){ .hw = GPIOC, .pin = 4 }, GPIO_FUNC_A);

    // LCD data RGB lanes
    gpio_set_func(&(struct gpio){ .hw = GPIOB, .pin = 11 }, GPIO_FUNC_A);
    gpio_set_func(&(struct gpio){ .hw = GPIOB, .pin = 12 }, GPIO_FUNC_A);
    gpio_set_func(&(struct gpio){ .hw = GPIOB, .pin = 13 }, GPIO_FUNC_A);
    gpio_set_func(&(struct gpio){ .hw = GPIOB, .pin = 14 }, GPIO_FUNC_A);
    gpio_set_func(&(struct gpio){ .hw = GPIOB, .pin = 15 }, GPIO_FUNC_A);
    gpio_set_func(&(struct gpio){ .hw = GPIOB, .pin = 16 }, GPIO_FUNC_A);
    gpio_set_func(&(struct gpio){ .hw = GPIOB, .pin = 17 }, GPIO_FUNC_A);
    gpio_set_func(&(struct gpio){ .hw = GPIOB, .pin = 18 }, GPIO_FUNC_A);

    gpio_set_func(&(struct gpio){ .hw = GPIOB, .pin = 19 }, GPIO_FUNC_A);
    gpio_set_func(&(struct gpio){ .hw = GPIOB, .pin = 20 }, GPIO_FUNC_A);
    gpio_set_func(&(struct gpio){ .hw = GPIOB, .pin = 21 }, GPIO_FUNC_A);
    gpio_set_func(&(struct gpio){ .hw = GPIOB, .pin = 22 }, GPIO_FUNC_A);
    gpio_set_func(&(struct gpio){ .hw = GPIOB, .pin = 23 }, GPIO_FUNC_A);
    gpio_set_func(&(struct gpio){ .hw = GPIOB, .pin = 24 }, GPIO_FUNC_A);
    gpio_set_func(&(struct gpio){ .hw = GPIOB, .pin = 25 }, GPIO_FUNC_A);
    gpio_set_func(&(struct gpio){ .hw = GPIOB, .pin = 26 }, GPIO_FUNC_A);

    gpio_set_func(&(struct gpio){ .hw = GPIOB, .pin = 27 }, GPIO_FUNC_A);
    gpio_set_func(&(struct gpio){ .hw = GPIOB, .pin = 28 }, GPIO_FUNC_A);
    gpio_set_func(&(struct gpio){ .hw = GPIOB, .pin = 29 }, GPIO_FUNC_A);
    gpio_set_func(&(struct gpio){ .hw = GPIOB, .pin = 30 }, GPIO_FUNC_A);
    gpio_set_func(&(struct gpio){ .hw = GPIOB, .pin = 31 }, GPIO_FUNC_A);
    gpio_set_func(&(struct gpio){ .hw = GPIOC, .pin = 0  }, GPIO_FUNC_A);
    gpio_set_func(&(struct gpio){ .hw = GPIOC, .pin = 1  }, GPIO_FUNC_A);
    gpio_set_func(&(struct gpio){ .hw = GPIOC, .pin = 2  }, GPIO_FUNC_A);
}

void lcd_clock_init(void)
{
    clk_pck_enable(45);
    clk_gck_enable(45, GCK_SRC_PLLA_CLK, 2);

    // The LCD clock requres specific initialization in the PMC
    PMC->SCER = (1 << 3);
    while (!(PMC->SCSR & (1 << 3)));
}

// Wait for the clock dimain syncronication to complete. Until this the main
// configuration registers cannot be accessed. 
static inline void wait_clock_domain_sync(void)
{
    while (LCD->LCDSR & (1 << 4));
}

void lcd_on(struct lcd_info* info)
{
    // 1. Configure LCD timing, signal polarity and clock period
    
    // Calculate the pixel clock
    u32 pixel_clock = info->framerate * (info->h_back_porch +
        info->h_front_porch + info->h_pulse_width + info->w) * 
        (info->v_back_porch + info->v_front_porch + info->v_pulse_width + 
        info->h);

    u8 div = (166000000 * 2) / pixel_clock - 1;

    // Wait for clock domain sync
    wait_clock_domain_sync();
    LCD->LCDCFG0 = (div << 16) | (1 << 3) | (1 << 2) | (0xF << 8) | (1 << 13);
    
    // Timing parameters
    wait_clock_domain_sync();
    LCD->LCDCFG1 = (((info->h_pulse_width - 1) & 0x3FF) << 0) | 
        (((info->v_pulse_width - 1) & 0x3FF) << 16);

    wait_clock_domain_sync();
    LCD->LCDCFG2 = (((info->v_front_porch - 1) & 0x3FF) << 0) | 
        ((info->v_back_porch & 0x3FF) << 16);

    wait_clock_domain_sync();
    LCD->LCDCFG3 = (((info->h_front_porch - 1) & 0x3FF) << 0) |
        (((info->h_back_porch - 1) & 0x3FF) << 16);

    wait_clock_domain_sync();
    LCD->LCDCFG4 = (((info->w - 1) & 0x3FF) << 0) |
        (((info->h - 1) & 0x3FF) << 16);

    // Setup guard time, clock polarity, and color mode
    wait_clock_domain_sync();
    LCD->LCDCFG5 = (1 << 16) | (1 << 0) | (1 << 1) | (1 << 2) | (1 << 7);

    // Set PWM mode
    wait_clock_domain_sync();
    LCD->LCDCFG6 = (0b110 << 0) | (1 << 4) | (0x10 << 8);
    
    // 2. Enable pixel clock LCDEN
    wait_clock_domain_sync();
    LCD->LCDEN = (1 << 0);

    // 2. Wait for clock to be stable
    while (!(LCD->LCDSR & (1 << 0)));

    // 3. Enable vertical and horizontal sychronization
    LCD->LCDEN = (1 << 1);

    // 4. Wait for the synch to complete
    while (!(LCD->LCDSR & (1 << 1)));

    // 5. Enable display power
    LCD->LCDEN = (1 << 2);

    // 6. Wait for power
    while (!(LCD->LCDSR & (1 << 2)));

    // Enable the PWM signal
    LCD->LCDEN = (1 << 3);
    while (!(LCD->LCDSR & (1 << 3)));
}

// Powerdown sequence for the LCD display
void lcd_off(void)
{
    // TODO fix the DMA unlinking here

    // Turn off the backlight
    LCD->LCDDIS = (1 << 3);
    while (LCD->LCDSR & (1 << 3));

    // 1. Disable the DISP signal
    LCD->LCDDIS = (1 << 2);

    // 2. Wait for the display to power down
    while (LCD->LCDDIS & (1 << 2));

    // 3. Disable the sync signals
    LCD->LCDDIS = (1 << 1);

    // 4. Wait for sync to stop
    while (LCD->LCDDIS & (1 << 1));

    // 5. Disable the pixel clock
    LCD->LCDDIS = (1 << 0);

    // 6. Wait for the pixel clock to stop
    while (LCD->LCDDIS & (1 << 0));
}

// This function initializes all the hardware layers and maps the buffers to 
// DMA structures
static void lcd_layer_init(void)
{
    // Assign a DMA descriptor to each layer framebuffer
    struct lcd_layer* layer = &lcd_layers[0];
    struct lcd_dma_desc* dma = &lcd_dma_descriptors[0];

    // Link the DMA descriptors to the framebuffers
    for (u32 i = 0; i < LAYERS; i++) {
        for (u32 j = 0; j < 2; j++) {

            // Compute the physical address of the DMA stuff
            layer->buffer_pa[j] = (u32)va_to_pa(layer->buffer[j]);
            layer->buffer_dma_pa[j] = (u32)va_to_pa(layer->buffer_dma[j]);

            // Link the DMA channel
            layer->buffer_dma[j] = dma;

            // Just clear the buffers
            mem_set(layer->buffer[j], 0x00, layer->info.bpp * layer->info.h *
                layer->info.w);

            dma++;
        }
        
        // TODO remove - framebuffers is non-cacheable
        dcache_clean();
        layer++;
    }

    // Assign the hardware registers to the LCD stuctures
    lcd_layers[0].cfg = LCD->BASECFG;
    lcd_layers[0].dma = &LCD->base_dma;
    lcd_layers[0].ctrl = &LCD->base_ctrl;

    lcd_layers[1].cfg = LCD->OV1CFG;
    lcd_layers[1].dma = &LCD->ov1_dma;
    lcd_layers[1].ctrl = &LCD->ov1_ctrl;

    lcd_layers[2].cfg = LCD->OV2CFG;
    lcd_layers[2].dma = &LCD->ov2_dma;
    lcd_layers[2].ctrl = &LCD->ov2_ctrl;
}

static void lcd_layer_enable(void)
{
    for (u32 i = 0; i < LAYERS; i++) {

        struct lcd_layer* layer = &lcd_layers[i];
        if (i == 0) {
            layer->cfg[0] = (3 << 4) | (1 << 8);  // 16 byte AHB burst
            layer->cfg[1] = (10 << 4);            // 24-bit BGR888 no lookup
            layer->cfg[2] = 0;                    // No X stride
            layer->cfg[3] = 0;                    // Default black
            layer->cfg[4] = (1 << 8);             // DMA enable
            layer->cfg[5] = 0;                    // Display position 0:0
            layer->cfg[6] = 0;
        } else {
            layer->cfg[0] = (3 << 4) | (1 << 12) | (1 << 8);
            layer->cfg[1] = (13 << 4);           // 32-bit ABGR8888 no lookup
            layer->cfg[2] = 0;                   // Window position
            layer->cfg[3] = (layer->info.w << 0) | (layer->info.h << 16);
            layer->cfg[4] = 0;                   // No X stride
            layer->cfg[5] = 0;                   // No pixel stride
            layer->cfg[6] = 0;                   // Default black
            layer->cfg[7] = 0;                   // Default chroma black
            layer->cfg[8] = 0;                   // No chroma mask
            layer->cfg[9] = (1 << 2) | (1 << 3) | (1 << 6) | (1 << 7) | (1 << 8);
        }
        struct lcd_dma_desc* dma_desc = layer->buffer_dma[0];

        // Setup the DMA descriptors
        for (u32 j = 0; j < 2; j++) {

            // Set up so that the same descriptor fetched the same buffer
            dma_desc->addr = layer->buffer_pa[j];
            dma_desc->next = layer->buffer_dma_pa[j];
            dma_desc->ctrl = 1;

            // Only the buffers are marked as non-cacheable so we still have to 
            // clean the DMA descriptor range
            dcache_clean_range((u32)dma_desc, (u32)dma_desc + 
                sizeof(struct lcd_dma_desc));

            dma_desc++;
        }

        // Beacuse of the weird interface of this LCD controller we start all
        // the DMA channels with descriptor 0. The procedure for starting and 
        // updating buffers are different, hence this method
        layer->dma->ADDR = layer->buffer_pa[0];
        layer->dma->NEXT = layer->buffer_dma_pa[0];
        layer->dma->CTRL = 1;

        layer->active_index = 0;
    }
}

void lcd_switch_screenbuffer(u8 layer)
{
    assert(layer < LAYERS);
    struct lcd_layer* curr_layer = &lcd_layers[layer];

    // Toggle the buffer index
    curr_layer->active_index ^= 1;

    curr_layer->dma->HEAD = curr_layer->buffer_dma_pa[curr_layer->active_index];
    curr_layer->ctrl->CHER = (1 << 2);

    // When this layer is fetched the old layer can be clared and reused. This
    // will stall the system. Replace with interrupt and worker threads
    while (!(curr_layer->ctrl->ISR & (1 << 4)));
}

struct screenbuffer* lcd_get_new_screenbuffer(u8 layer)
{
    assert(layer < LAYERS);
    struct lcd_layer* curr_layer = &lcd_layers[layer];
    
    // Wait for the new framebuffer to be linked with the DMA
    while (curr_layer->ctrl->CHER & (1 << 2));

    curr_layer->info.buffer = curr_layer->buffer[curr_layer->active_index ^ 1];

    return &curr_layer->info;
}

static struct lcd_info lcd_info = {
    .h             = 480,
    .w             = 800,
    .framerate     = 60,
    .v_back_porch  = 21,
    .v_front_porch = 22,
    .v_pulse_width = 2,
    .h_back_porch  = 64,
    .h_front_porch = 64,
    .h_pulse_width = 128
};

void lcd_init(void)
{
    assert(LAYERS == 3);
    lcd_pin_init();
    lcd_clock_init();

    // Initialize the layers
    lcd_layer_init();

    // Enable the layers
    lcd_layer_enable();

    lcd_layers[0].ctrl->CHER = 0b11;
    lcd_layers[1].ctrl->CHER = 0b11;
    lcd_layers[2].ctrl->CHER = 0b11;

    lcd_on(&lcd_info);
    lcd_set_brightness(0xFF);
}

// Sets the display backlight intesnsity
void lcd_set_brightness(u8 brightness)
{
    u32 reg = LCD->LCDCFG6;
    reg &= ~0xFF00;
    reg |= (brightness << 8);
    LCD->LCDCFG6 = reg;
}

void lcd_get_size(u16* x, u16* y, u8 layer)
{
    *y = lcd_layers[layer].info.h;
    *x = lcd_layers[layer].info.w;
}
