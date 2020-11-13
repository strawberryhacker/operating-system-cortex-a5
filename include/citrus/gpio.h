/* Copyright (C) strawberryhacker */

#ifndef GPIO_H
#define GPIO_H

#include <citrus/types.h>
#include <citrus/regmap.h>

enum gpio_func {
    GPIO_FUNC_OFF,
    GPIO_FUNC_A,
    GPIO_FUNC_B,
    GPIO_FUNC_C,
    GPIO_FUNC_D,
    GPIO_FUNC_E,
    GPIO_FUNC_F,
    GPIO_FUNC_G
};

enum gpio_dir {
    GPIO_INPUT,
    GPIO_OUTPUT
};

enum gpio_pull {
    GPIO_PULLUP,
    GPIO_PULLDOWN,
    GPIO_NO_PULL
};

enum gpio_drive {
    GPIO_DIRVE_LO,
    GPIO_DRIVE_ME,
    GPIO_DRIVE_HI
};

enum gpio_event {
    GPIO_EVENT_FALLING,
    GPIO_EVENT_RISING,
    GPIO_EVENT_BOTH,
    GPIO_EVENT_LOW,
    GPIO_EVENT_HIGH
};

struct gpio {
    struct gpio_reg* hw;
    u32 pin;
};

struct gpio_conf {
    enum gpio_func func;
    enum gpio_dir dir;
    enum gpio_event event;
    enum gpio_pull pull;
    enum gpio_drive drive;
};

void gpio_init(const struct gpio* gpio, const struct gpio_conf* conf); 
void gpio_set_func(const struct gpio* gpio, enum gpio_func func);
void gpio_set_dir(const struct gpio* gpio, enum gpio_dir dir);
void gpio_set_event(const struct gpio* gpio, enum gpio_event event);
void gpio_set_pull(const struct gpio* gpio, enum gpio_pull pull);
void gpio_set_drive(const struct gpio* gpio, enum gpio_drive drive);
void gpio_set(const struct gpio* gpio);
void gpio_clear(const struct gpio* gpio);
void gpio_toggle(const struct gpio* gpio);
u32 gpio_get_out(const struct gpio* gpio);
u32 gpio_get_in(const struct gpio* gpio);
void gpio_irq_enable(const struct gpio* gpio);

#endif