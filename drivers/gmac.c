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
#include <citrus/panic.h>
#include <citrus/cache.h>
#include <net/netbuf.h>
#include <citrus/print.h>

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

static struct netbuf* tx_netbuf[TX_SIZE];
static struct netbuf* rx_netbuf[RX_SIZE];

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
    na_rx_desc = cache_alloc(NA_CNT * 8, 8);
    na_tx_desc = cache_alloc(NA_CNT * 8, 8);

    tx_desc    = cache_alloc(TX_DESC_CNT * 8, 8);
    rx_desc    = cache_alloc(RX_DESC_CNT * 8, 8);

    tx_buf     = cache_alloc(TX_SIZE * TX_DESC_CNT, 32);
    rx_buf     = cache_alloc(RX_SIZE * RX_DESC_CNT, 32);
    
    // Allocate the buffers for the receive queue
    for (u32 i = 0; i < RX_DESC_CNT; i++) {
        struct netbuf* buf = alloc_netbuf();

        if (buf == NULL)
            panic("Cant allocate netbuffers");
        
        rx_netbuf[i] = buf;
    }

    // Initialzie the rx netbuffer queue
    for (u32 i = 0; i < TX_DESC_CNT; i++) {
        tx_netbuf[i] = NULL;
    }
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

    wait_phy_idle();

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
        tx_desc[i].addr = 0;
        tx_desc[i].status_word = 0;
        tx_desc[i].owner_software = 1;
    }
    tx_desc[TX_DESC_CNT - 1].wrap = 1;

    // Main receive buffer. We have to make sure that the GMAC is owning the 
    // buffers
    for (u32 i = 0; i < RX_DESC_CNT; i++) {
        rx_desc[i].addr_word = (u32)va_to_pa(rx_netbuf[i]->buf);
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



// This needs to take in the maximum size of the network buffer which is 1500
i32 gmac_rec_raw(struct netbuf** buf)
{
    static u32 ii = 0;

    struct netbuf* curr_buf = rx_netbuf[rx_index];
    struct gmac_rx_desc* desc = &rx_desc[rx_index];


    // Check the current desc status bif    
    if (desc->owner_software == 1) {

        // Allocate a new netbuffer
        struct netbuf* new_netbuf = alloc_netbuf();

        curr_buf->frame_len = desc->length;
        curr_buf->ptr = curr_buf->buf;

        // Invalidate the bufferpart of the netbuf
        dcache_invalidate_range((u32)curr_buf->buf, 
            (u32)(curr_buf->buf + desc->length));

        // Link in the new netbuf
        desc->addr = (u32)va_to_pa(new_netbuf->buf) >> 2;

        // Swap the entriees inthe netbuf queue
        rx_netbuf[rx_index] = new_netbuf;

        // We have rad one entry, so we have to move the rx index pointer
        if (++rx_index >= RX_DESC_CNT)
            rx_index = 0;
        
        // Return the current buffer
        *buf = curr_buf;

        // Transfer ownershiåp to the GMAC DMA
        desc->owner_software = 0;

        return 0;
    }

    return -ERETRY;
}

// Takes in a buffer with data and queues it for transmission
void gmac_send_raw(struct netbuf* buf)
{
    struct gmac_tx_desc* desc = &tx_desc[tx_index];

    // Find the first descriptor which the software owns
    if (desc->owner_software == 0)
        panic("TX error");
    
    // Check if the transfer descriptor is already pointing to a netbuffer
    if (rx_netbuf[tx_index]) {
        free_netbuf(rx_netbuf[tx_index]);
    }

    rx_netbuf[tx_index] = buf;

    // We have to map in the physical buffer
    desc->addr = (u32)va_to_pa(buf->ptr);

    // Clean the D-cache
    //dcache_clean();
    dcache_clean_range((u32)buf->ptr, (u32)buf->ptr + buf->frame_len);

    // Double check the framelength
    assert(buf->frame_len < NETBUF_LENGTH);

    // Mark the descriptor as the last one. We never use more than one 
    // descriptor in this setup
    desc->length = buf->frame_len;
    desc->last_buffer = 1;

    // Mark the buffer as owned by the NIC
    desc->owner_software = 0;

    // Enable the transmitter. Note: this might allready be enabled
    GMAC->NCR |= (1 << 9);

    // Wait for the transmite to complete
    while (!(GMAC->TSR & (1 << 5)));

    // TX complete flag is now set
    if (GMAC->TSR & ((1 << 4) | (1 << 8) | 0b110)) {
        panic("TX error status");
    }

    // Check the CRC flags
    if (desc->crc_gen_err) {
        print("CRC ERROR => %03b\n", desc->crc_gen_err);
    }

    // Increase the tx index after the transfer
    if (++tx_index == TX_DESC_CNT)
        tx_index = 0;
    
}

void gmac_init(void)
{
    netbuf_init();

    gmac_clk_init();
    gmac_port_init();

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

    // Divide the 63 MHz cgmac_get_mac_addrll frames with bad CRC
    GMAC->NCFGR |= 0b11 | (1 << 4) | (1 << 17) | (1 << 24);

    // Choose RMII operation
    GMAC->UR = 1;

    // Setup the DMA registers. The receive buffer size if 128 bytes
    GMAC->DCFGR = (3 << 8) | (0x18 << 16) | (1 << 0) | (1 << 11);

    gmac_init_queues();

    // Enable the MDC port, the transmitter and the reciever
    GMAC->NCR = 0b111 << 2;

    // Configure the PHY. THis must be done after power-on but can be removed
    // after (to speed things up)
    phy_setup();

    GMAC->NCR |= (1 << 9);
}

const u8 mac_addr[6] = { 0x3c, 0x97, 0x0e, 0x2f, 0x43, 0x6c };

const u8* gmac_get_mac_addr(void)
{
    return mac_addr;
}