/* Copyright (C) strawberryhacker */

#include <citrus/gpio.h>

static inline void gpio_mask(const struct gpio* gpio)
{
    gpio->hw->MSKR = (1 << gpio->pin);
}

void gpio_init(const struct gpio* gpio, const struct gpio_conf* conf)
{
    gpio_mask(gpio);
    u32 cfg = (conf->dir << 8) | (conf->drive << 16) | (conf->event << 24) |
              (conf->func << 0);
    
    if (conf->pull == GPIO_PULLDOWN) {
        cfg |= (1 << 10);
    } else if (conf->pull == GPIO_PULLUP) {
        cfg |= (1 << 9);
    }
    gpio->hw->CFGR = cfg;
} 

void gpio_set_func(const struct gpio* gpio, enum gpio_func func)
{
    gpio_mask(gpio);
    u32 cfg = gpio->hw->CFGR;
    cfg &= ~0b111;
    cfg |= func;
    gpio->hw->CFGR = cfg;
}

void gpio_set_dir(const struct gpio* gpio, enum gpio_dir dir)
{
    gpio_mask(gpio);
    u32 cfg = gpio->hw->CFGR;
    cfg &= ~(1 << 8);
    cfg |= (dir << 8);
    gpio->hw->CFGR = cfg;
}

void gpio_set_event(const struct gpio* gpio,
    enum gpio_event event)
{
    gpio_mask(gpio);
    u32 cfg = gpio->hw->CFGR;
    cfg &= ~(0b111 << 24);
    cfg |= (event << 24);
    gpio->hw->CFGR = cfg;
}

void gpio_set_pull(const struct gpio* gpio, enum gpio_pull pull)
{
    gpio_mask(gpio);
    u32 cfg = gpio->hw->CFGR;
    cfg &= ~(0b11 << 9);

    if (pull == GPIO_PULLDOWN) {
        cfg |= (1 << 10);
    } else if (pull == GPIO_PULLUP) {
        cfg |= (1 << 9);
    }
    gpio->hw->CFGR = cfg;
}

void gpio_set_drive(const struct gpio* gpio, enum gpio_drive drive)
{
    gpio_mask(gpio);
    u32 cfg = gpio->hw->CFGR;
    cfg &= ~(0b11 << 16);
    cfg |= (drive << 16);
    gpio->hw->CFGR = cfg;
}

void gpio_set(const struct gpio* gpio)
{
    gpio->hw->SODR = (1 << gpio->pin);
}

void gpio_clear(const struct gpio* gpio)
{
    gpio->hw->CODR = (1 << gpio->pin);
}

void gpio_toggle(const struct gpio* gpio)
{
    if (gpio->hw->ODSR & (1 << gpio->pin)) {
        gpio->hw->CODR = (1 << gpio->pin);
    } else {
        gpio->hw->SODR = (1 << gpio->pin);
    }
}

/*
 * Returns the value driven on the GPIO
 */
u32 gpio_get_out(const struct gpio* gpio)
{
    return gpio->hw->ODSR;
}

/*
 * Returns the value present on the GPIO
 */
u32 gpio_get_in(const struct gpio* gpio)
{
    return gpio->hw->PDSR;
}

void gpio_irq_enable(const struct gpio* gpio)
{
    gpio->hw->IER = (1 << gpio->pin);
}
