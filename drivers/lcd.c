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

void lcd_irq_init(void)
{

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
    LCD->LCDCFG5 = (30 << 16) | (1 << 0) | (1 << 1) | (1 << 2) | (1 << 7) |
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

void lcd_init(void)
{
    lcd_pin_init();
    lcd_clock_init();
}

// Sets the display backlight intensity
void lcd_set_brightness(u8 brightness)
{
    u32 reg = LCD->LCDCFG6;
    reg &= ~0xFF00;
    reg |= (brightness << 8);
    LCD->LCDCFG6 = reg;
}

struct lcd_dma_desc alignas(8) desc;
struct lcd_dma_desc alignas(8) desc1;

volatile struct rgb framebuffer[800*480];
volatile struct rgb framebuffer1[800*480];
volatile struct rgb window[300*200];

struct lcd_dma_desc alignas(8) wdesc;

void test(void)
{
    LCD->base_ctrl.IDR = 0xFF;
    desc.next = (u32)va_to_pa((void *)&desc);
    desc.addr = (u32)va_to_pa((void *)framebuffer);
    desc.ctrl = (1 << 0);
    dcache_clean();
    LCD->base_dma.NEXT = (u32)va_to_pa((void *)&desc);
    LCD->base_dma.ADDR = (u32)va_to_pa((void *)framebuffer);
    LCD->base_dma.CTRL = (1 << 0);
    // 16 data
    LCD->BASECFG[0] = (1 << 8) | (3 << 4);
    LCD->BASECFG[1] = (10 << 4);
    LCD->BASECFG[4] = (1 << 8) | (1 << 9);
    LCD->base_ctrl.CHER = (0b11 << 0);   
}
