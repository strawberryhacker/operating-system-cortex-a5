// Copyright (C) strawberryhacker 

#include <citrus/print.h>
#include <citrus/sprint.h>
#include <citrus/gpio.h>
#include <citrus/clock.h>
#include <citrus/interrupt.h>
#include <citrus/regmap.h>

static char buffer[1024];

void print(const char* data, ...)
{
    va_list args;
    va_start(args, data);
    u32 cnt = vsprint(buffer, data, args);
    va_end(args);

    const u8* start = (const u8 *)buffer;
    while (cnt--) {
        while (!(UART1->SR & (1 << 1)));
        UART1->THR = *start;
        start++;
    }
}

static char buffer_task[1024];

void print_init(void)
{
    // Setup the pins
    gpio_set_func(&(struct gpio){ .hw = GPIOB, .pin = 3 }, GPIO_FUNC_A);
    gpio_set_func(&(struct gpio){ .hw = GPIOB, .pin = 4 }, GPIO_FUNC_A);

    clk_pck_enable(28);

    struct uart_reg* const uart = UART4;

    uart->MR = (4 << 9);
    uart->BRGR = 23;
    uart->CR = (1 << 6);
}

void print_task(const char* data, ...)
{
    va_list args;
    va_start(args, data);
    u32 cnt = vsprint(buffer_task, data, args);
    va_end(args);

    const u8* start = (const u8 *)buffer_task;
    while (cnt--) {
        while (!(UART4->SR & (1 << 1)));
        UART4->THR = *start;
        start++;
    }
}
