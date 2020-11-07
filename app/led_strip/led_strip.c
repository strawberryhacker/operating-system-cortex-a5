/// Copyright (C) strawberryhacker

#include <app/led_strip.h>

#include <citrus/thread.h>
#include <citrus/print.h>
#include <citrus/gpio.h>
#include <citrus/clock.h>
#include <citrus/interrupt.h>
#include <citrus/spi.h>
#include <graphics/engine.h>
#include <citrus/syscall.h>

struct pixel strip[16];

void led_strip_loop(void)
{

}

static struct engine e;

/// Main LED strip thread
u32 led_strip_thread(void* args)
{
    print("LED strip thread started\n");

    // Setup pins
    gpio_set_func(&(struct gpio){ .hw = GPIOD, .pin = 25 }, GPIO_FUNC_A);
    gpio_set_func(&(struct gpio){ .hw = GPIOD, .pin = 26 }, GPIO_FUNC_A);

    // Enable the SPI clock
    clk_pck_enable(34);
    spi_init();

    for (u32 i = 0; i < 16; i++){
        strip[i] = (struct pixel){.glob = 15, .green = 5, .blue = 0, .red = 0 };
    }

    led_strip_update(strip, 16);

    engine_init(&e);

    while (1) {
        led_strip_loop();
    }
    
    return 1;
}

/// Created the LED strip thread
void led_strip_init(void)
{
    create_kernel_thread(led_strip_thread, 500, "ledstrip", NULL, SCHED_RT);
}

/// Updates the led strip
void led_strip_update(struct pixel* pixels, u32 cnt)
{
    for (u32 i = 0; i < 4; i++) {
        spi_out(0x00);
    }

    for (u32 i = 0; i < cnt; i++) {
        spi_out((0b111 << 5) | pixels->glob);
        spi_out(pixels->blue);
        spi_out(pixels->green);
        spi_out(pixels->red);
    }

    for (u32 i = 0; i < 4; i++) {
        spi_out(0xFF);
    }
}
