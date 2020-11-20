// Copyright (C) strawberryhacker

#include <citrus/mmc.h>
#include <citrus/print.h>
#include <citrus/panic.h>
#include <citrus/gpio.h>
#include <citrus/clock.h>
#include <citrus/apic.h>
#include <citrus/thread.h>
#include <citrus/kmalloc.h>
#include <stddef.h>

// This code is configuring the MMC driver to work in SD / SDIO mode

// Sets the bus width of the MMC hardware. Currently only 1 and 8 bit supported
static void mmc_set_bus_width(struct sd_card* sd, u32 bus_width)
{
    struct mmc_reg* mmc = sd->mmc;

    assert(bus_width == 4 || bus_width == 1);

    if (bus_width == 1) {
        mmc->HC1R &= ~(1 << 1);
    } else {
        mmc->HC1R |= (1 << 1);
    }
}

// Enables the high-speed MMC mode
static void mmc_set_high_speed(struct sd_card* sd, u32 high_speed)
{
    struct mmc_reg* mmc = sd->mmc;

    if (high_speed) {
        mmc->HC1R |= (1 << 2);
    } else {
        mmc->HC1R &= ~(1 << 2);
    }
}

// The SD host has a register to choose the SDCLK clock frequency. SD host 
// version 1.00 and 2.00 embeds a 8 bit divider while the version 3.00
// increases this number to 10 bits. 
//
// According to the SD card spesification the maximum speed during 
// configuration is 400kHz and the max operating frequency is 25 MHz in normal 
// speed mode and 50MHz is high speed mode.
//
// The version 3.00 host controller supports arbritary prescaling given by the
// formula target_clock = base_clock / (DIV + 1). The capabilities register 
// shows wheather this clock mode is supported. A non-zero value in this
// register indicates this
static void mmc_set_frequency(struct sd_card* sd, u32 frequency)
{
    struct mmc_reg* mmc = sd->mmc;

    // We should be able to use the programmable clock mode. In this case the 
    // mult field in the capabilities register has to be non-zero
    assert((mmc->CA1R >> 16) & 0xFF);

    // The frequency select is done by calculating the 10-bit SDCLKFSEL value.
    u16 freq = 0;
    u32 prog_clk = 1;

    // The datasheet page 1790 says f_sdclk = f_multclk / (DIV + 1). The GCK is
    // configured to have a frequency of 166.000 / 2 = 83 MHz
    freq = (83 * 1000 * 1000 / frequency) - 1;

    // Stop the clock first
    mmc->CCR &= ~(1 << 2);

    // Change the configuration
    u32 reg = mmc->CCR;
    reg &= ~(0x3FF << 6);
    reg |= (freq << 8) | (((freq >> 8) & 0b11) << 6);

    // We are using the programmable clock
    reg |= (1 << 5);

    // Update config and enable internal clock
    mmc->CCR = reg | (1 << 0);

    // Wait for the internal clock to be ready
    while (!(mmc->CCR & (1 << 1)));
    
    // Enable the SD clock
    mmc->CCR |= (1 << 2);
}

// Set bus power 
static void mmc_set_bus_power(struct mmc_reg* mmc)
{    
    // Get the supported bus power
    mmc->PCR |= (1 << 0);
}

static void mmc_init_hardware(struct mmc_reg* mmc)
{
    // Perform a full software reset of the MMC module
    mmc->SRR |= (1 << 0);
    while (mmc->SRR & 1);
}

// Enabled interrupt on card insert
static void mmc_card_detect_irq_enable(struct mmc_reg* mmc)
{
    mmc->NISTER |= (1 << 6) | (1 << 7);
    mmc->NISIER |= (1 << 6) | (1 << 7);

    // Clear the status
    mmc->NISTR |= (1 << 6) | (1 << 7);
}

// Creates a new SD card and initializes the SD card structure
static struct sd_card* mmc_create_sd_card(void)
{
    struct sd_card* card = kzmalloc(sizeof(struct sd_card));

    card->write = mmc_send_command;
    card->set_frequency = mmc_set_frequency;
    card->set_bus_width = mmc_set_bus_width;
    card->set_high_speed = mmc_set_high_speed;

    return card;
}

// SD protocol thread function that will enumerate and add a new card
extern i32 sd_init_thread(void* sd_card);

// Handles the event of a SD insertion. This creates a new SD card object and 
// creates a new thread which will enumerate the newly attached SD card
static void mmc_card_insert(struct mmc_reg* mmc)
{
    // Check the card insertion in the present state. TODO debounce isssue
    assert(mmc->PSR & (1 << 16));

    // Card has been inserted and everything is ok. The host controller can 
    // apply power on the signal lines and enable the clock
    mmc_set_bus_power(mmc);

    // Fix the standard interrupt flags
    mmc->NISIER = (1 << 6) | (1 << 7);
    mmc->EISIER = 0;

    mmc->EISTER = 0xFFFF;
    mmc->NISTER = 0b111011 | (1 << 6) | (1 << 7) | (1 << 15);

    // A new card has been inserted
    struct sd_card* card = mmc_create_sd_card();

    // Set the right private interface
    card->mmc = mmc;

    // Create a new user (kernel) thread to enumerate the card
    create_kthread(sd_init_thread, 500, "sdinit", card, SCHED_RT);
}

// Internal interrupt handler which will handle both requests from MMC0 and
// MMC1 and call the appropriate handlers
static void _mmc_interrupt_handler(struct mmc_reg* mmc, u32 status)
{
    if (status & (1 << 6)) {
        // Card insertion
        mmc_card_insert(mmc);
    }

    if (status & (1 << 7)) {
        panic("Ejection not supported");
    }
}

// Main MMC interrupt for MMC0
void mmc0_interrupt(void)
{
    // Read and clear the status bits
    u32 status = MMC0->NISTR;
    MMC0->NISTR = status;

    // Call the common interrupt handler
    _mmc_interrupt_handler(MMC0, status);
}

// Main MMC interrupt for MMC1
void mmc1_interrupt(void)
{
    // Read and clear the status bits
    u32 status = MMC1->NISTR;
    MMC1->NISTR = status;

    // Call the common interrupt handler
    _mmc_interrupt_handler(MMC1, status);
}

void mmc_init(void)
{
    clk_pck_enable(31);
    clk_pck_enable(32);

    // Enable the programmable clock for the SD host; 10,375 MHz. This is used 
    // to gernerate the SDCLK
    clk_gck_enable(31, GCK_SRC_MCK_CLK, 2);
    clk_gck_enable(32, GCK_SRC_MCK_CLK, 2);

    // Add interrupt handlers for both MMC interfaces
    apic_add_handler(31, mmc0_interrupt);
    apic_enable(31);

    apic_add_handler(32, mmc1_interrupt);
    apic_enable(32);

    // Initialize the hardware
    mmc_init_hardware(MMC1);
    mmc_init_hardware(MMC0);

    // This driver depends on the ADMA to be supported
    assert(MMC0->CA0R & (1 << 19));
    assert(MMC1->CA0R & (1 << 19));

    // Initialize the GPIO    
    gpio_set_func(&(struct gpio){ .hw = GPIOA, .pin = 30 }, GPIO_FUNC_E);
    gpio_set_func(&(struct gpio){ .hw = GPIOA, .pin = 18 }, GPIO_FUNC_E);
    gpio_set_func(&(struct gpio){ .hw = GPIOA, .pin = 19 }, GPIO_FUNC_E);
    gpio_set_func(&(struct gpio){ .hw = GPIOA, .pin = 20 }, GPIO_FUNC_E);
    gpio_set_func(&(struct gpio){ .hw = GPIOA, .pin = 21 }, GPIO_FUNC_E);
    gpio_set_func(&(struct gpio){ .hw = GPIOA, .pin = 22 }, GPIO_FUNC_E);
    gpio_set_func(&(struct gpio){ .hw = GPIOA, .pin = 28 }, GPIO_FUNC_E);

    gpio_set_func(&(struct gpio){ .hw = GPIOA, .pin = 2  }, GPIO_FUNC_A);
    gpio_set_func(&(struct gpio){ .hw = GPIOA, .pin = 3  }, GPIO_FUNC_A);
    gpio_set_func(&(struct gpio){ .hw = GPIOA, .pin = 4  }, GPIO_FUNC_A);
    gpio_set_func(&(struct gpio){ .hw = GPIOA, .pin = 5  }, GPIO_FUNC_A);
    gpio_set_func(&(struct gpio){ .hw = GPIOA, .pin = 1  }, GPIO_FUNC_A);
    gpio_set_func(&(struct gpio){ .hw = GPIOA, .pin = 0  }, GPIO_FUNC_A);
    gpio_set_func(&(struct gpio){ .hw = GPIOA, .pin = 13 }, GPIO_FUNC_A);


    // Enable interrupt on card detect
    mmc_card_detect_irq_enable(MMC0);
    mmc_card_detect_irq_enable(MMC1);
}

// Internal structure for speeding up command 17 and command 18 transfers by
// use of the ADMA
struct mmc_adma {
    u16 attr;
    u16 length;
    u32 addr;
} __attribute__ ((packed, aligned(4)));

static void mmc_wait_cmd_inhibit(struct mmc_reg* mmc)
{
    // Wait for the command line to be idle (wait for command inhibit)
    u32 timeout = 10000;
    while ((--timeout) && mmc->PSR & 0b11);

    if (!timeout) {
        panic("Cant send command");
    }
}

// Returns the cmd register based on the cmd index, the response type and if 
// there is any data stage at all
static u32 mmc_construct_cmd_reg(u32 cmd_index, u32 resp, struct mmc_data* data)
{
    // Set the command index
    u32 cmd_reg = (cmd_index & 0b111111) << 8;

    // Assume CRC7
    cmd_reg |= (1 << 3);

    // Response 3 does not have a CRC7
    if (resp == SD_RESP_R3) {
        cmd_reg &= ~(1 << 3);
    }

    // Set the response type
    if (resp == SD_RESP_R1b) {
        cmd_reg |= 3;
    } else if (resp == SD_RESP_R2){
        cmd_reg |= 1;
    } else if (resp != SD_RESP_NONE) {
        cmd_reg |= 2;
    }

    // R3 and R2 does not have a command index echo
    cmd_reg |= (1 << 4);
    if (resp == SD_RESP_R2 || resp == SD_RESP_R3) {
        cmd_reg &= ~(1 << 4);
    }

    return cmd_reg;
}

// Reads raw data after a SD command
static u32 mmc_read_raw(struct mmc_reg* mmc, struct mmc_data* data)
{
    if (data->dir == 1) {
        return 0;
    }

    u32 status;
    u32 blocks = 0;
    u32* dest = (u32 *)data->data;
    u32 timeout = 100000;

    do {
        status = mmc->NISTR;

        if (status & (1 << 15)) {
            panic("Error trying to read data");
        }

        // Check if we are done with all the blocks
        if (blocks >= data->blocks) {
            break;
        }

        // Wait for buffer read ready
        if ((status & (0b1 << 5)) && (mmc->PSR & (1 << 11))) {
            for (u32 i = 0; i < data->block_size; i += 4) {
                *dest++ = mmc->BDPR;
            }
            blocks++;
            continue;
        }

        if (--timeout == 0) {
            print("Blcoks => %d\n", blocks);
            print("Error => %016b\n", mmc->EISTR);
            print("Statu => %016b\n", mmc->NISTR);
            panic("Timeout");
        }

    } while (!(status & (1 << 1)));

    timeout = 1000;

    u32 error = mmc->EISTR;
    
    // Reset the data and command line
    mmc->SRR |= (1 << 1);
    while (mmc->SRR & (1 << 1));
    mmc->SRR |= (1 << 2);
    while (mmc->SRR & (1 << 2));

    mmc->EISTR = error;
    mmc->NISTR = status;

    return 1;
}

// Performs a send command operation
u32 mmc_send_command(struct sd_card* sd, struct mmc_cmd* cmd, struct mmc_data* data)
{
    struct mmc_reg* mmc = sd->mmc;

    // If ADMA is used for transfer
    struct mmc_adma adma[16];

    // Wait for the command line to be idle
    mmc_wait_cmd_inhibit(mmc);

    // Construct the command register based on the parameters
    u32 cmd_reg = mmc_construct_cmd_reg(cmd->cmd, cmd->resp_type, data);

    // Command complete
    u32 status_mask = (1 << 0);

    // Wait for busy to be deasserted in case of R1b
    if (cmd->resp_type == SD_RESP_R1b) {
        status_mask |= (1 << 1);
    }

    // Check if we have a data stage present
    if (data) {
        // Data is present
        cmd_reg |= (1 << 5);

        // Set the transfer mode register. We enable block count by default 
        // since the ADMA does not support infinite memory transfers
        u32 mode_reg = (1 << 1);

        // Set the mode direction and the block mode
        if (data->dir == 0) {
            mode_reg |= (1 << 4);
        }
        if (data->blocks) {
            mode_reg |= (1 << 5);
        }
        
        // Enable the ADMA for command 17 and command 18
        if (cmd->cmd == 18 || cmd->cmd == 18) {
            u32 reg = mmc->HC1R & ~(0b11 << 3);
            mmc->HC1R = reg | (2 << 3);

            // DMA enable in the transfer register
            mode_reg |= (1 << 0);
        } else {

        }

        // Set the timeout control register
        mmc->TCR = 0xE;

        mmc->BSR = data->block_size;
        if (data->blocks) {
            mmc->BCR = data->blocks;
        }

        // Write the mode register 
        mmc->TMR = mode_reg;

        // Set up the DMA for command 17 and command 18
        if (cmd->cmd == 18 || cmd->cmd == 18) {

            if (data->blocks > 16) {
                panic("Too many blocks requested");
            }

            // Setup the ADMA table
            for (u32 i = 0; i < data->blocks; i++) {
                print("ADMA table\n");
                if (i == data->blocks - 1) {
                    // Mark this block as the end of the transfer
                    adma[i].attr = 0x23;
                } else {
                    adma[i].attr = 0x21;
                }     
                adma[i].length = data->block_size;
                adma[i].addr = (u32)(data->data + data->block_size * i);
            }

            // Write the ADMA start address in the register
            mmc->ASAR = (u32)adma;
        }
    }

    // Set the argument register
    mmc->ARG1R = cmd->arg;

    // At this point we send the command and wait for the command to complete
    mmc->CR = cmd_reg;

    u32 status;
    u32 timeout = 10000;
    do {
        status = mmc->NISTR;
    } while ((--timeout) && (status & status_mask) != status_mask);

    if (!timeout) {
        panic("Timeout during command completion");
    }

    // Clear the status bits except the buffer read and write ready flags, which
    // will be cleared by the read and write data routine
    mmc->NISTR = status & ~(0b11 << 4);

    // Read the response
    if (cmd->resp_type == SD_RESP_R2) {
        // We have to read the entire 136 bit response
        for (u32 i = 0; i < 4; i++) {
            cmd->resp[i] = mmc->RR[i];
        }
    } else {
        cmd->resp[0] = mmc->RR[0];
    }

    if (data) {
        if (cmd->cmd == 18 || cmd->cmd == 18) {
            // The ADMA will handle command 17 and command 18
            timeout = 10000;
            do {
                status = mmc->NISTR;
            } while ((--timeout) && !(status & (1 << 1)));

            // The transfer complete flag is set
            if (!timeout) {
                print("Desc => %p\n", mmc->AESR);
                print("Desc => %p\n", mmc->ASAR);
                
                struct mmc_adma* a = (struct mmc_adma *)mmc->ASAR;
                print("A => %p\n", a->addr);
                print("A => %05b\n", a->attr);
                print("A => %p\n", a->length); 
                print("Error => %016b\n", mmc->EISTR);
                print("Statu => %016b\n", mmc->NISTR);
                panic("Timeout on advanced DMA read");
            }

            // Clear the flag
            mmc->NISTR = (1 << 1);

            if (mmc->NISTR & (1 << 15)) {
                panic("We have errors");
            }
        } else {
            // We manually read the data using the data buffer port
            status = mmc_read_raw(mmc, data);

            if (!status) {
                panic("Failed to read data");
            }
        }
        
    }

    return 1;
}
