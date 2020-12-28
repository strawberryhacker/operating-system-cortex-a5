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
#define NA_CNT  6

#define RX_SIZE 1536
#define TX_SIZE 1536

#define RX_DESC_CNT 4
#define TX_DESC_CNT 32

// Dummy buffers for GMAC unused queues
static u8 (*na_tx_buf)[NA_SIZE];
static u8 (*na_rx_buf)[NA_SIZE];

// Main transmit and receive buffer pool. This is dynamically allocated from the
// non-cache allocator
static u8 (*tx_buf)[TX_SIZE];
static u8 (*rx_buf)[RX_SIZE];

// Ring descriptors pointing to the buffers. These are accessed by the CPU as 
// well as the GMAC DMA controller
static struct gmac_tx_desc* na_tx_desc;
static struct gmac_rx_desc* na_rx_desc;
static struct gmac_tx_desc* tx_desc;
static struct gmac_rx_desc* rx_desc;

static volatile u32 rx_index;
static volatile u32 tx_index;

// Allocates non-cacheable buffers for trasmitt and receive queues, both used 
// and not used. This also allocates the buffer descriptors from non-cacheable
// memory. This means that cache maintance is unnecessary. It is still necessary
// to convert to physical addresses since the allocatings produce a VA
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
static inline void wait_phy_idle(void)
{
    while (!(GMAC->NSR & (1 << 2)));
}

// Reads a register from the addressed phy
u16 phy_read(u8 addr, u8 reg)
{
    u32 tmp = 0;
    wait_phy_idle();

    // Clause 22 and read operation
    GMAC->MAN = (0b10 << 16) | (1 << 30) | (0b10 << 28) | 
        ((addr & 0x1F) << 23) | ((reg & 0x1F) << 18);

    wait_phy_idle();

    // Read the result
    return (u16)GMAC->MAN;
}

// Write data into one of the PHY registers
void phy_write(u8 addr, u8 reg, u16 data)
{
    wait_phy_idle();

    // Clause 22 write operation
    GMAC->MAN = (0b10 << 16) | (1 << 30) | (0b01 << 28) | 
        ((addr & 0x1F) << 23) | ((reg & 0x1F) << 18) | data;

    // Read the register to make sure it has been written
    if (data != phy_read(addr, reg))
        print("ERROR\n");
}

// Given the PHY ID, this functions searches through all the 32 possible PHY
// addresses and tries to find the address of this phy. 
// 
// Returns 0 if the address is found and -ENODEV if the PHY does not exist
i32 phy_discover(u16 id, u16* addr)
{
    for (u32 i = 0; i < 32; i++) {
        if (phy_read(i, 2) == id) {
            *addr = i;
            return 0;
        }
    }
    return -ENODEV;
}

// Configures the PHY to operate in 100Base-T mode and does some other 
// configuration as well
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

// Configures the PHY for 100Base-T operation and waits for link up
static void phy_setup(void)
{
    // Try to get the address of the PHY with address 0x0022
    u16 addr = 0;
    i32 err = phy_discover(0x0022, &addr);
    if (err)
        return;
    
    // Wait for the link status from the PHY
    u32 reg;
    do {
        reg = phy_read(addr, 1);
    } while (!(reg & (1 << 2)));

    // Find the mode if the PHY

    phy_enable_100baseT(addr);
    
    print("Link is up\n");
}

// Configures the pins connected to the ethernet PHY. This is using a RMII
// interface. 
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

// Enables the clock of the GMAC peripheral. It seems it's only using the 
// peripheral clock
static void gmac_clk_init(void)
{
    clk_pck_enable(5);
}

static void gmac_init_queues(void)
{
    tx_index = 0;
    rx_index = 0;

    // Main transmit buffer. We have to make sure we are owning the buffers
    for (u32 i = 0; i < TX_DESC_CNT; i++) {
        tx_desc[i].addr = (u32)va_to_pa(tx_buf[i]);
        tx_desc[i].status_word = 0;
        tx_desc[i].owner_software = 1;
    }
    tx_desc[TX_DESC_CNT - 1].wrap = 1;

    // Main receive buffer. We have to make sure that the GMAC is owning the 
    // buffers
    for (u32 i = 0; i < RX_DESC_CNT; i++) {
        rx_desc[i].addr_word = (u32)va_to_pa(rx_buf[i]);
        rx_desc[i].status_word = 0;
    }

    // Mark the buffer as the last one in the queue
    rx_desc[RX_DESC_CNT - 1].wrap = 1;

    // Not used buffer for the queues. These has to be set up regardless
    for (u32 i = 0; i < 3; i++) {
        na_rx_desc[i].addr_word = (u32)va_to_pa(na_rx_buf[i]);
        na_tx_desc[i].addr = (u32)va_to_pa(na_tx_buf[i]);
        na_rx_desc[i].status_word = GMAC_RX_WRAP_MASK;
        na_tx_desc[i].status_word = GMAC_TX_USED_MASK | GMAC_TX_WRAP_MASK;
    }

    // Write the physical base address into the registers
    GMAC->TBQB = (u32)va_to_pa(tx_desc);
    GMAC->RBQB = (u32)va_to_pa(rx_desc);

    for (u32 i = 0; i < 3; i++) {
        GMAC->TBQBAPQ[i] = (u32)va_to_pa(na_tx_desc + i);
        GMAC->RBQBAPQ[i] = (u32)va_to_pa(na_rx_desc + i);
    }
}

// Main IRQ for the GMAC
static void gmac_irq(void)
{
    print("GMAC interrupt\n");

    u32 isr = GMAC->ISR;
    print("status register => %032b\n", isr);
    print("Tx status       => %032b\n", GMAC->TSR);
    if (isr & (1 << 7)) {
        //print("Yess\n");
    }

    return;

    for (u32 i = 0; i < RX_DESC_CNT; i++) {
        print("Status %d > ", i);
        if (rx_desc[i].status_word & GMAC_RX_SOF_MASK)
            print("SOF ");
        if (rx_desc[i].status_word & GMAC_RX_EOF_MASK)
            print("EOF");
        print("\n");
    }

    print("Len first buffer => %d\n", rx_desc[0].status_word & GMAC_RX_FRAME_LEN_MASK);
    mem_dump(rx_buf[0], rx_desc[0].status_word & GMAC_RX_FRAME_LEN_MASK, 10, 1);
}

// This needs to take in the maximum size of the network buffer which is 1500
void gmac_receive(void)
{
    static u8 ii = 0;

    // Check if a new packet is available in the packet buffer
    u32 status = GMAC->RSR;
    if (status & (1 << 1)) {

        // Clear the receive flags by writing one
        GMAC->RSR = status;

        for (u32 i = 0; i < RX_DESC_CNT; i++) {

            // Get the buffer from the descriptor entry
            u8* tmp = rx_buf[rx_index];

            // Start checking on the current index
            struct gmac_rx_desc* desc = &rx_desc[rx_index++];

            // Check if the buffer index is too big
            if (rx_index == RX_DESC_CNT)
                rx_index = 0;

            // Check if the current descriptor is owned by software or if it
            // is owned by the GMAC DMA itself
            if (desc->owner_software == 1) {


                // We can control the buffer
                print("Got a packet: [");
                for (u32 ii = 0; ii < desc->length; ii++)
                    print("0x%02X, ", tmp[ii]);
                print("]\n");

                // Print the length
                print("Length: %d\n\n", desc->length);
                // Now we have to clear the owner flag
                desc->owner_software = 0;
                

                // If the buffer is used we are going to return that buffer so
                // we don't check the other buffers
                break;
            }
        }
    } 
}

// Takes in a buffer with data and queues it for transmission
void nic_send_raw(const u8* data, u32 len)
{
    print("\n");
    // For now the NIC will wait for the packet to be sent on the network. It 
    // will choose a free descriptor, but this will allmost allways be the 
    // first one since we are waiting for the package to be transmitted
    print("status; ");
    for (u32 i = 0; i < TX_DESC_CNT; i++)
        print("%d ", tx_desc[i].owner_software);
    print("\n");

    u8 i = tx_index;

    // Find the first descriptor which the software owns
    if (tx_desc[i].owner_software == 0)
        while (1);
    
    print("Found a free buffer at index %d\n", i);

    // Copy data from the input and into the NIC queue
    mem_copy(data, &tx_buf[i], len);

    // Mark the buffer as owned by the NIC
    tx_desc[i].owner_software = 0;

    // Mark the descriptor as the last one. We never use more than one 
    // descriptor in this setup
    tx_desc[i].length = len;
    tx_desc[i].last_buffer = 1;

    // Enable the transmitter
    GMAC->NCR |= (1 << 9);

    print("tx is enabled\n");
    while (GMAC->TSR == 0);

    // Check the status of the current buffer

    print("status after tx: %d\n", tx_desc[i].owner_software);

    // Increase the tx index after the transfer
    if (++tx_index == TX_DESC_CNT)
        tx_index = 0;

    return;
}

void gmac_init(void)
{
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
    GMAC->DCFGR = (3 << 8) | (0x18 << 16) | (1 << 0);

    gmac_init_queues();

    // Enable the MDC port, the transmitter and the reciever
    GMAC->NCR = 0b111 << 2;

    // Configure the PHY. THis must be done after power-on but can be removed
    // after (to speed things up)
    //phy_setup();

    // Check if the wrap bit is being set
    GMAC->NCR |= (1 << 9);
}