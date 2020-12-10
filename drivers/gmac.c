#include <citrus/gmac.h>
#include <citrus/print.h>
#include <citrus/clock.h>
#include <citrus/gpio.h>
#include <citrus/apic.h>
#include <citrus/regmap.h>
#include <citrus/error.h>
#include <stddef.h>

// Busy waiting for the PHY management logic to become idle
static void wait_phy_idle(void)
{
    while (!(GMAC->NSR & (1 << 2)));
}

// Reads a register from the addressed phy
u16 phy_read(u8 addr, u8 reg)
{
    wait_phy_idle();

    u32 tmp = 0;
    tmp |= (0b10 << 16);          // Mandatory
    tmp |= (1 << 30);             // Clause 22 operation
    tmp |= (0b10 << 28);          // Read operation
    tmp |= ((addr & 0x1F) << 23);
    tmp |= ((reg & 0x1F) << 18);
    GMAC->MAN = tmp;

    wait_phy_idle();

    return (u16)GMAC->MAN;
}

// Write data into one of the PHY registers
void phy_write(u8 addr, u8 reg, u16 data)
{
    wait_phy_idle();

    u32 tmp = 0;
    tmp |= (0b10 << 16);          // Mandatory
    tmp |= (1 << 30);             // Clause 22 operation
    tmp |= (0b01 << 28);          // Write operation
    tmp |= ((addr & 0x1F) << 23);
    tmp |= ((reg & 0x1F) << 18);
    tmp |= data;
    GMAC->MAN = tmp;
}

// Given the PHY ID, this functions searches through all the 32 possible PHY
// addresses and tries to find the address of this phy. 
// 
// Returns 0 if the address is found and -ENODEV if the PHY does not exist
i32 phy_discover(u16 id, u16* addr)
{
    for (u32 i = 0; i < 32; i++) {
        u16 tmp = phy_read(i, 2);
        if (tmp == id) {
            *addr = i;
            return 0;
        }
    }
    return -ENODEV;
}

static void gmac_port_init(void)
{
    gpio_set_func(&(struct gpio){ .hw = GPIOD, .pin = 9  }, GPIO_FUNC_D);
    gpio_set_func(&(struct gpio){ .hw = GPIOD, .pin = 10 }, GPIO_FUNC_D);
    gpio_set_func(&(struct gpio){ .hw = GPIOD, .pin = 11 }, GPIO_FUNC_D);
    gpio_set_func(&(struct gpio){ .hw = GPIOD, .pin = 12 }, GPIO_FUNC_D);
    gpio_set_func(&(struct gpio){ .hw = GPIOD, .pin = 13 }, GPIO_FUNC_D);
    gpio_set_func(&(struct gpio){ .hw = GPIOD, .pin = 14 }, GPIO_FUNC_D);
    gpio_set_func(&(struct gpio){ .hw = GPIOD, .pin = 15 }, GPIO_FUNC_D);
    gpio_set_func(&(struct gpio){ .hw = GPIOD, .pin = 16 }, GPIO_FUNC_D);
    gpio_set_func(&(struct gpio){ .hw = GPIOD, .pin = 17 }, GPIO_FUNC_D);
    gpio_set_func(&(struct gpio){ .hw = GPIOD, .pin = 18 }, GPIO_FUNC_D);
}

static void gmac_clk_init(void)
{
    clk_pck_enable(5);
}

// Main IRQ for the GMAC
static void gmac_irq(void)
{
    print("GMAC interrupt\n");
    while (1);
}

void gmac_init(void)
{
    print("Hello GMACX\n");

    gmac_clk_init();
    gmac_port_init();

    // All GMAC initializeation must be done with transmit and receive circuits
    // disables
    GMAC->NCR = 0;
    GMAC->NCFGR = 0;

    // Disable all interrupts
    GMAC->IDR = 0xFFFFFFFF;
    GMAC->IDRPQ[0] = 0xFFFFFFFF;
    GMAC->IDRPQ[1] = 0xFFFFFFFF;

    // Clear all the interrupt statuses
    (void)GMAC->ISR;
    (void)GMAC->ISRPQ[0];
    (void)GMAC->ISRPQ[1];

    // Clear all the status register bits
    GMAC->TSR = (1 << 8) | 0x3F;
    GMAC->RSR = 0xF;

    // Divide the 63 MHz clock by 32 to yield the aprox. 2 MHz MDC clock
    GMAC->NCFGR = (2 << 18);

    // Enable the MDC port and the transmitter and the reciever
    GMAC->NCR = 0b111 << 2;

    u16 addr = 0xFFFF;
    phy_discover(0x0022, &addr);
    print("Addr > %d\n", addr);
}
