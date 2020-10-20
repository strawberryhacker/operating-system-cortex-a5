/// Copyright (C) strawberryhacker

#include <cinnamon/mmc.h>
#include <cinnamon/print.h>
#include <cinnamon/panic.h>
#include <cinnamon/gpio.h>
#include <cinnamon/clock.h>

/// This code is configuring the MMC driver to work in SD / SDIO mode

/// Sets the bus width of the MMC hardware. Currently only 1 and 8 bit supported
static void mmc_set_bus_width(struct mmc_reg* mmc, u32 bus_width)
{
    assert(bus_width == 4 || bus_width == 1);

    if (bus_width == 1) {
        mmc->HC1R &= ~(1 << 1);
    } else {
        mmc->HC1R |= (1 << 1);
    }
}

/// Enables the high-speed MMC mode
static void mmc_set_high_speed(struct mmc_reg* mmc, u32 high_speed)
{
    if (high_speed) {
        mmc->HC1R |= (1 << 2);
    } else {
        mmc->HC1R &= ~(1 << 2);
    }
}

static void mmc_set_frequency(struct mmc_reg* mmc, u32 frequency)
{
    print("CCR => %d\n", mmc->CA0R >> 8 & 0xFF);
}

static void mmc_init_hardware(struct mmc_reg* mmc)
{
    // Perform a full software reset of the MMC module
    mmc->SRR |= (1 << 0);
    while (mmc->SRR & 1);
}

void mmc_init(struct mmc_reg* mmc)
{
    print("MMC starting\n");

    (void)mmc->CA0R;
    print("OK\n");

    clk_pck_enable(32);

    mmc_init_hardware(mmc);


    // Initialize the GPIO
    struct gpio cd   = { .hw = GPIOA, .pin = 30 };
    struct gpio dat0 = { .hw = GPIOA, .pin = 18 };
    struct gpio dat1 = { .hw = GPIOA, .pin = 19 };
    struct gpio dat2 = { .hw = GPIOA, .pin = 20 };
    struct gpio dat3 = { .hw = GPIOA, .pin = 21 };
    struct gpio ck   = { .hw = GPIOA, .pin = 22 };
    struct gpio cda  = { .hw = GPIOA, .pin = 28 };
    
    gpio_set_func(&dat0, GPIO_FUNC_E);
    gpio_set_func(&dat1, GPIO_FUNC_E);
    gpio_set_func(&dat2, GPIO_FUNC_E);
    gpio_set_func(&dat3, GPIO_FUNC_E);
    gpio_set_func(&ck, GPIO_FUNC_E);
    gpio_set_func(&cd, GPIO_FUNC_E);
    gpio_set_func(&cda, GPIO_FUNC_E);

    mmc_set_bus_width(mmc, 1);

    mmc_set_frequency(mmc, 500);
}
