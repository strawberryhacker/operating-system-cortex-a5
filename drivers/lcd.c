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
#include <citrus/regmap.h>
#include <stddef.h>
#include <stdalign.h>

#define SCREEN_X 800
#define SCREEN_Y 480
#define LAYERS 3

struct lcd_layer {
    struct lcd_dma_desc* dma_desc;
    u32* framebuffer;
    u16 width;
    u16 height;

    // Registes
    volatile struct lcd_ctrl* ctrl;
    volatile struct lcd_dma* dma;
    volatile u32* cfg;
};

// Framebuffer for all the layers. The layer0 is for background and static
// stuff, layer 1 is for the applications and the GUI engine, layer2 is for 
// mouse and overlays while layer3 is for special use. 
u32 alignas(32) framebuffers[LAYERS][SCREEN_X * SCREEN_Y];
struct lcd_dma_desc alignas(32) dma_descriptors[LAYERS];
struct lcd_layer layers[LAYERS];

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
    // ID 45
    clk_pck_enable(45);
    clk_gck_enable(45, GCK_SRC_PLLA_CLK, 2);

    // Enable the LCD system clock
    PMC->SCER = (1 << 3);
    while (!(PMC->SCSR & (1 << 3)));
}

static inline void wait_clock_domain_sync(void)
{
    while (LCD->LCDSR & (1 << 4));
}

// This function must be given the right parameters.
void lcd_on(struct lcd_info* info)
{
    // 1. Configure LCD timing, signal polarity and clock period
    
    // Calculate the pixel clock
    u32 pixel_clock = info->framerate * (info->h_back_porch +
        info->h_front_porch + info->h_pulse_width + info->width) * 
        (info->v_back_porch + info->v_front_porch + info->v_pulse_width + 
        info->height);

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
    LCD->LCDCFG4 = (((info->width - 1) & 0x3FF) << 0) |
        (((info->height - 1) & 0x3FF) << 16);

    // Setup guard time, clock polarity, and color mode
    wait_clock_domain_sync();
    LCD->LCDCFG5 = (1 << 16) | (1 << 0) | (1 << 1) | (1 << 2) | (1 << 7) |
        (3 << 8);

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

u32 get_color(u8 r, u8 g, u8 b, u8 a)
{
    u32 reg = ((r >> 2) << 20) | ((g >> 2) << 14) | ((b >> 2) << 8) | a;

    return reg;
}

void set_color(u32* buffer, u16 x, u16 y, u16 w, u16 h, u32 color)
{
    u32 (*data)[800] = (u32 (*)[800])buffer;
    for (u32 i = x; i < x + w; i++) {
        for (u32 j = y; j < y + h; j++) {
            data[j][i] = color;
        }
    }
    dcache_clean();
}

static void lcd_layer_init(void)
{
    for (u32 i = 0; i < LAYERS; i++) {
        layers[i].framebuffer = framebuffers[i];
        layers[i].dma_desc = &dma_descriptors[i];
        layers[i].height = SCREEN_Y;
        layers[i].width = SCREEN_X;

        mem_set(framebuffers[i], 0x00, 4 * SCREEN_Y * SCREEN_X);
        dcache_clean_range((u32)framebuffers[i], 
            (u32)framebuffers[i] + SCREEN_X * SCREEN_Y);
    }

    layers[0].cfg = LCD->BASECFG;
    layers[0].dma = &LCD->base_dma;
    layers[0].ctrl = &LCD->base_ctrl;

    layers[1].cfg = LCD->OV1CFG;
    layers[1].dma = &LCD->ov1_dma;
    layers[1].ctrl = &LCD->ov1_ctrl;

    layers[2].cfg = LCD->OV2CFG;
    layers[2].dma = &LCD->ov2_dma;
    layers[2].ctrl = &LCD->ov2_ctrl;
}

static void lcd_layer_enable(void)
{
    assert(LAYERS < 4);

    for (u32 i = 0; i < LAYERS; i++) {
        // Set up the layer
        volatile u32* cfg = layers[i].cfg;

        // Base layer
        if (i == 0) {
            cfg[0] = (3 << 4) | (1 << 8);  // 16 byte AHB burst
            cfg[1] = (13 << 4);            // 24-bit RGB no lookup
            cfg[2] = 0;                    // No X stride
            cfg[3] = 0;                    // Default black when DMA is disabled
            cfg[4] = (1 << 8);             // DMA enable
            cfg[5] = 0;                    // Display position 0:0
            cfg[6] = 0;
        } else {
            cfg[0] = (3 << 4) | (0b1 << 12) | (1 << 8);  // 16 byte AHB burst - ROT disable
            cfg[1] = (13 << 4);           // 24-bit RGB no lookup
            cfg[2] = 0;                   // Window position

            // Windows size
            cfg[3] = (layers[i].width << 0) | (layers[i].height << 16);

            cfg[4] = 0;                   // No X stride
            cfg[5] = 0;                   // No pixel stride
            cfg[6] = 0;                   // Default black
            cfg[7] = 0;                   // Default chroma black
            cfg[8] = 0xFFFFFF;            // Default mask black
            cfg[9] = (1 << 8) | (0xFF << 16) | (0 << 5) | (1 << 7) | (1 << 2) | (1 << 3)| (1 << 6);
        }

        // Set up the DMA descriptor
        struct lcd_dma_desc* desc = layers[i].dma_desc;

        desc->addr = (u32)va_to_pa(layers[i].framebuffer);
        desc->next = (u32)va_to_pa(desc);
        desc->ctrl = 1;

        dcache_clean_range((u32)desc, (u32)desc + sizeof(struct lcd_dma_desc));

        // Setup the DMA registers
        volatile struct lcd_dma* dma = layers[i].dma;

        dma->ADDR = (u32)va_to_pa(layers[i].framebuffer);
        dma->NEXT = (u32)va_to_pa(desc);
        dma->CTRL = 1;
    }
}

u32* lcd_get_framebuffer(u8 layer)
{
    assert(layer < 4);
    return layers[layer].framebuffer;
}

static struct lcd_info lcd_info = {
    .height        = 480,
    .width         = 800,
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

    lcd_on(&lcd_info);

    layers[0].ctrl->CHER = 0b11;
    layers[1].ctrl->CHER = 0b11;
    layers[2].ctrl->CHER = 0b11;

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
