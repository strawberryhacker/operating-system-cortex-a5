/// Copyright (C) strawberryhacker

#include <app/led_strip.h>

#include <citrus/thread.h>
#include <citrus/print.h>
#include <citrus/gpio.h>
#include <citrus/clock.h>
#include <citrus/interrupt.h>
#include <citrus/spi.h>
#include <citrus/syscall.h>

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
    
    return 1;
}

/// Created the LED strip thread
void led_strip_init(void)
{
    create_kernel_thread(led_strip_thread, 500, "ledstrip", NULL, SCHED_RT);
}
