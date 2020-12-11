#include <citrus/gmac.h>
#include <citrus/print.h>
#include <citrus/clock.h>
#include <citrus/gpio.h>
#include <citrus/apic.h>
#include <citrus/regmap.h>
#include <citrus/error.h>
#include <citrus/cache.h>
#include <citrus/mm.h>
#include <citrus/mem.h>
#include <stddef.h>
#include <stdalign.h>

#define RX_SIZE 128
#define TX_SIZE 1500

#define RX_DESC_CNT 32
#define TX_DESC_CNT 2

static u8 alignas(32) na_tx_buf[2][4];
static u8 alignas(32) na_rx_buf[2][4];
static u8 alignas(32) tx_buf[TX_DESC_CNT][TX_SIZE];
static u8 alignas(32) rx_buf[RX_DESC_CNT][RX_SIZE];

static struct gmac_desc alignas(8) na_tx_desc[2];
static struct gmac_desc alignas(8) na_rx_desc[2];
static struct gmac_desc alignas(8) tx_desc[TX_DESC_CNT];
static struct gmac_desc alignas(8) rx_desc[RX_DESC_CNT];

static volatile u32 rx_index;
static volatile u32 tx_index;

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

void phy_enable_100baseT(u8 addr)
{
    // Read the auto-negotiate register
    u16 reg = phy_read(addr, 4);
    reg |= (1 << 8);

    // Page 28 in the datasheet reccomends clearing this bit
    reg &= ~(1 << 15);
    phy_write(addr, 4, reg);

    // Restart the auto-negotiation
    reg = phy_read(addr, 0);
    reg |= (1 << 9);
    phy_write(addr, 0, reg);

    // Wait for the auto-negotiation to complete
    u32 counter = 0;
    do {
        reg = phy_read(addr, 1);
        counter++;
    } while (!(reg & (1 << 5)));

    // Wait for link
    do {
        reg = phy_read(addr, 1);
    } while (!(reg & (1 << 2)));
}

void phy_get_cfg(u8 addr, enum phy_speed* speed, enum phy_duplex* dup)
{
    // Get the operating mode
    u16 reg = phy_read(addr, 0x1E);

    print("Status => %03b\n", reg & 0b111);
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

    print("status register => %032b\n", GMAC->ISR);

    dcache_invalidate_range((u32)rx_desc, (u32)(rx_desc + RX_DESC_CNT + 1));

    for (u32 i = 0; i < RX_DESC_CNT; i++) {
        print("Status %d > ", i);
        if (rx_desc[i].status & GMAC_RX_SOF_POS)
            print("SOF ");
        if (rx_desc[i].status & GMAC_RX_EOF_POS)
            print("EOF");
        print("\n");
    }

    print("Len first buffer => %d\n", rx_desc[0].status & GMAC_RX_FRAME_LEN_MASK);
    mem_dump(rx_buf[0], rx_desc[0].status & GMAC_RX_FRAME_LEN_MASK, 10, 1);

    while (1);
}

static void gmac_init_queues(void)
{
    tx_index = 0;
    rx_index = 0;

    // Main transmit buffer
    for (u32 i = 0; i < TX_DESC_CNT; i++) {
        tx_desc[i].addr = (u32)va_to_pa(tx_buf[i]);
        tx_desc[i].status = GMAC_TX_USED_POS;   // We own the buffer
    }
    tx_desc[TX_DESC_CNT - 1].status |= GMAC_TX_WRAP_POS;

    // Main receive buffer
    for (u32 i = 0; i < RX_DESC_CNT; i++) {
        rx_desc[i].addr = (u32)va_to_pa(rx_buf[i]);
        rx_desc[i].status = 0;                  // The GMAC own the buffer
    }
    rx_desc[RX_DESC_CNT - 1].status |= GMAC_RX_WRAP_POS;

    // Not used buffer for the queues. These has to be set up regardless
    for (u32 i = 0; i < 2; i++) {
        na_rx_desc[i].addr = (u32)va_to_pa(na_rx_buf[i]);
        na_tx_desc[i].addr = (u32)va_to_pa(na_tx_buf[i]);
        na_rx_desc[i].status = GMAC_RX_WRAP_POS;
        na_tx_desc[i].status = GMAC_TX_USED_POS | GMAC_TX_WRAP_POS;
    }

    dcache_clean();

    // Write the base address into the registers
    GMAC->TBQB = (u32)va_to_pa(tx_desc);
    GMAC->RBQB = (u32)va_to_pa(rx_desc);
    for (u32 i = 0; i < 2; i++) {
        GMAC->TBQBAPQ[i] = (u32)va_to_pa(na_tx_desc + i);
        GMAC->RBQBAPQ[i] = (u32)va_to_pa(na_rx_desc + i);
    }
}

void gmac_init(void)
{
    print("Hello GMACX\n");

    gmac_clk_init();
    gmac_port_init();

    // Enable the GMAC interrupts
    apic_add_handler(5, gmac_irq);
    apic_enable(5);

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

    // For now just set the 100Base operation and full-duplex mode
    GMAC->NCFGR |= 0b11 | (1 << 4);

    // Choose RMII operation
    GMAC->UR = 1;

    // Setup the DMA registers. The receive buffer size if 128 bytes
    GMAC->DCFGR |= (3 << 8) | (0x02 << 16);

    gmac_init_queues();

    // Enable some interrupts
    GMAC->IER = (1 << 1) | (1 << 10);

    // Enable the MDC port and the transmitter and the reciever
    GMAC->NCR = 0b111 << 2;
}
