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
#include <citrus/cache_alloc.h>
#include <stddef.h>
#include <stdalign.h>

#define NA_SIZE 128
#define NA_CNT 6

#define RX_SIZE 128
#define TX_SIZE 1500

#define RX_DESC_CNT 32
#define TX_DESC_CNT 2

static u8 (*na_tx_buf)[NA_SIZE];
static u8 (*na_rx_buf)[NA_SIZE];
static u8 (*tx_buf)[TX_SIZE];
static u8 (*rx_buf)[RX_SIZE];

static struct gmac_desc* na_tx_desc;
static struct gmac_desc* na_rx_desc;
static struct gmac_desc* tx_desc;
static struct gmac_desc* rx_desc;

static volatile u32 rx_index;
static volatile u32 tx_index;

static void gmac_alloc_buffers(void)
{
    na_rx_buf  = cache_alloc(NA_SIZE * NA_CNT, 32);
    na_tx_buf  = cache_alloc(NA_SIZE * NA_CNT, 32);
    tx_buf     = cache_alloc(TX_SIZE * TX_DESC_CNT, 32);
    rx_buf     = cache_alloc(RX_SIZE * RX_DESC_CNT, 32);
    na_rx_desc = cache_alloc(NA_CNT * 8, 8);
    na_tx_desc = cache_alloc(NA_CNT * 8, 8);
    tx_desc    = cache_alloc(TX_DESC_CNT * 8, 8);
    rx_desc    = cache_alloc(RX_DESC_CNT * 8, 8);
}

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

    u32 isr = GMAC->ISR;
    print("status register => %032b\n", isr);
    print("Tx status       => %032b\n", GMAC->TSR);
    if (isr & (1 << 7)) {
        print("Yess\n");
        while (1);
    }

    return;

    for (u32 i = 0; i < RX_DESC_CNT; i++) {
        print("Status %d > ", i);
        if (rx_desc[i].status & GMAC_RX_SOF_MASK)
            print("SOF ");
        if (rx_desc[i].status & GMAC_RX_EOF_MASK)
            print("EOF");
        print("\n");
    }

    print("Len first buffer => %d\n", rx_desc[0].status & GMAC_RX_FRAME_LEN_MASK);
    mem_dump(rx_buf[0], rx_desc[0].status & GMAC_RX_FRAME_LEN_MASK, 10, 1);
}

static void gmac_init_queues(void)
{
    tx_index = 0;
    rx_index = 0;

    // Main transmit buffer
    for (u32 i = 0; i < TX_DESC_CNT; i++) {
        tx_desc[i].addr = (u32)va_to_pa(tx_buf[i]);
        tx_desc[i].status = GMAC_TX_USED_MASK;   // We own the buffer
    }
    tx_desc[TX_DESC_CNT - 1].status |= GMAC_TX_WRAP_MASK;

    // Main receive buffer
    for (u32 i = 0; i < RX_DESC_CNT; i++) {
        rx_desc[i].addr = (u32)va_to_pa(rx_buf[i]);
        rx_desc[i].status = 0;                  // The GMAC own the buffer
    }
    rx_desc[RX_DESC_CNT - 1].status |= GMAC_RX_WRAP_MASK;

    // Not used buffer for the queues. These has to be set up regardless
    for (u32 i = 0; i < 2; i++) {
        na_rx_desc[i].addr = (u32)va_to_pa(na_rx_buf[i]);
        na_tx_desc[i].addr = (u32)va_to_pa(na_tx_buf[i]);
        na_rx_desc[i].status = GMAC_RX_WRAP_MASK;
        na_tx_desc[i].status = GMAC_TX_USED_MASK | GMAC_TX_WRAP_MASK;
    }

    // Write the base address into the registers
    GMAC->TBQB = (u32)va_to_pa(tx_desc);
    GMAC->RBQB = (u32)va_to_pa(rx_desc);

    for (u32 i = 0; i < 3; i++) {
        GMAC->TBQBAPQ[i] = (u32)va_to_pa(na_tx_desc + i);
        GMAC->RBQBAPQ[i] = (u32)va_to_pa(na_rx_desc + i);
    }
}

static void phy_setup(void)
{
    print("Starting network thread\n");
 
    // Try to get the address of the PHY with address 0x0022
    u16 addr = 0;
    i32 err = phy_discover(0x0022, &addr);
    if (err)
        return;
    
    print("Addr > %d\n", addr);

    // Wait for the link status from the PHY
    u32 reg;
    do {
        reg = phy_read(addr, 1);
    } while (!(reg & (1 << 2)));

    print("Link is up\n");

    // Find the mode if the PHY
    phy_enable_100baseT(addr);
}

void gmac_init(void)
{
    print("Hello GMACX\n");

    gmac_clk_init();
    gmac_port_init();

    // Enable the GMAC interrupts
    apic_add_handler(5, gmac_irq);
    apic_enable(5);

    // Allocate non-cacheable buffers
    gmac_alloc_buffers();

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
    GMAC->DCFGR = (3 << 8) | (0x02 << 16) | (1 << 0);

    gmac_init_queues();

    // Enable some interrupts
    GMAC->IER = 0xFFFFFFFF << 1;

    // Enable the MDC port, the transmitter and the reciever
    GMAC->NCR = 0b111 << 2;

    //phy_setup();

    const char msg[] = "\xff\xff\xff\xff\xff\xff\x00\xe0\x4c\x36\x14\x96\x08\x00\x45\x10" \
"\x01\x48\x00\x00\x00\x00\x80\x11\x39\x96\x00\x00\x00\x00\xff\xff" \
"\xff\xff\x00\x44\x00\x43\x01\x34\x4a\x42\x01\x01\x06\x00\xAA\xAA" \
"\xAA\xAA\x00\x19\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
"\x00\x00\x00\x00\x00\x00\x00\xe0\x4c\x36\x14\x96\x00\x00\x00\x00" \
"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
"\x00\x00\x00\x00\x00\x00\x63\x82\x53\x63\x35\x01\x01\x0c\x04\x68" \
"\x6f\x6d\x65\x37\x10\x01\x1c\x02\x03\x0f\x06\x77\x0c\x2c\x2f\x1a" \
"\x79\x2a\xf9\x21\xfc\xff\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00" \
"\x00\x00\x00\x00\x00\x00";

    u32 msg_size = sizeof(msg) - 1;
    print("Size of => %d\n", msg_size);

    for (u32 i = 0; i < msg_size; i++) {
        tx_buf[0][i] = 0xAA;
    }

    tx_desc[0].status |= GMAC_TX_LAST_MASK | msg_size;
    tx_desc[0].status &= ~GMAC_TX_USED_MASK;

    asm volatile ("dsb");

    print("DONE\n");
    GMAC->NCR |= (1 << 9);
}
