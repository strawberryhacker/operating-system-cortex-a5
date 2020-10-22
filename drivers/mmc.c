/// Copyright (C) strawberryhacker

#include <cinnamon/mmc.h>
#include <cinnamon/print.h>
#include <cinnamon/panic.h>
#include <cinnamon/gpio.h>
#include <cinnamon/clock.h>
#include <cinnamon/apic.h>
#include <cinnamon/thread.h>
#include <stddef.h>

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

/// The SD host has a register to choose the SDCLK clock frequency. SD host 
/// version 1.00 and 2.00 embeds a 8 bit divider while the version 3.00
/// increases this number to 10 bits. 
///
/// According to the SD card spesification the maximum speed during 
/// configuration is 400kHz and the max operating frequency is 25 MHz in normal 
/// speed mode and 50MHz is high speed mode.
///
/// The version 3.00 host controller supports arbritary prescaling given by the
/// formula target_clock = base_clock / (DIV + 1). The capabilities register 
/// shows wheather this clock mode is supported. A non-zero value in this
/// register indicates this
static void mmc_set_frequency(struct mmc_reg* mmc, u32 frequency)
{
    // We should be able to use the programmable clock mode. In this case the 
    // mult field in the capabilities register has to be non-zero
    assert((mmc->CA1R >> 16) & 0xFF);

    // The frequency select is done by calculating the 10-bit SDCLKFSEL value.
    u16 freq = 0;
    u32 prog_clk = 1;

    // The datasheet page 1790 says f_sdclk = f_multclk / (DIV + 1). The GCK is
    // configured to have a frequency of 166.000 / 16 = 10 MHz
    freq = (10 * 1000 * 1000 / frequency) - 1;

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

/// Set bus power 
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

/// Enabled interrupt on card insert
static void mmc_card_detect_irq_enable(struct mmc_reg* mmc)
{
    mmc->NISTER |= (1 << 6) | (1 << 7);
    mmc->NISIER |= (1 << 6) | (1 << 7);

    // Clear the status
    mmc->NISTR |= (1 << 6) | (1 << 7);
}

/// SD protocol thread function that will enumerate and add a new card
extern u32 sd_init_thread(void* sd);

/// Handles the event of a SD insertion. This creates a new SD card object and 
/// creates a new thread which will enumerate the newly attached SD card
static void mmc_card_insert(struct mmc_reg* mmc)
{
    // Check the card insertion in the present state
    assert(mmc->PSR & (1 << 16));

    // Card has been inserted and everything is ok. The host controller can 
    // apply power on the signal lines and enable the clock
    mmc_set_bus_power(mmc);

    // Fix the standard interrupt flags
    mmc->NISIER = (1 << 6) | (1 << 7);
    mmc->EISIER = 0;

    mmc->EISTER = 0xFFFF;
    mmc->NISTER = 0b111011 | (1 << 6) | (1 << 7) | (1 << 15);

    // Create a new user (kernel) thread to enumerate the card
    create_process(sd_init_thread, 500, "sdinit", NULL, SCHED_RT);
}

/// Internal interrupt handler which will handle both requests from MMC0 and
/// MMC1 and call the appropriate handlers
static void _mmc_interrupt_handler(struct mmc_reg* mmc, u32 status)
{
    if (status & (1 << 6)) {
        // Card insertion
        print("Card insertion\n");
        mmc_card_insert(mmc);
    }

    if (status & (1 << 7)) {
        print("Card removal\n");
    }
}

/// Main MMC interrupt for MMC0
void mmc0_interrupt(void)
{
    // Read and clear the status bits
    u32 status = MMC0->NISTR;
    MMC0->NISTR = status;

    // Call the common interrupt handler
    _mmc_interrupt_handler(MMC0, status);
}

/// Main MMC interrupt for MMC1
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

    // Enable the programmable clock for the SD host; 10,375 MHz
    clk_gck_enable(31, GCK_SRC_MCK_CLK, 16);
    clk_gck_enable(32, GCK_SRC_MCK_CLK, 16);

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

/// Internal structure for speeding up command 17 and command 18 transfers by
/// use of the ADMA
struct mmc_adma {
    u16 attr;
    u16 length;
    u32 addr;
};

/// Performs a send command operation
void mmc_send_command(struct mmc_reg* mmc, struct mmc_cmd* cmd, struct mmc_data* data)
{
    // If ADMA is used for transfer
    struct mmc_adma adma[16];

    // Wait for the command line to be idle (wait for command inhibit)
    u32 timeout = 10000;
    while ((--timeout) && mmc->PSR & 0b11);

    if (!timeout) {
        panic("Cant send command");
    }

    // First part is just constructing the command field. This depends on the 
    // response type as well as the command index

    // Set the command index
    u32 cmd_reg = (cmd->cmd & 0b111111) << 8;

    // Allways listen for command complete
    u32 status_mask = (1 << 0);

    // Assume CRC7
    cmd_reg |= (1 << 3);

    // Response 3 does not have a CRC7
    if (cmd->resp_type == SD_RESP_R3) {
        cmd_reg &= ~(1 << 3);
    }

    // Set the response type
    if (cmd->resp_type == SD_RESP_R1b) {
        cmd_reg |= 3;

        // R1b requires us to wait for the TRFC to complete in the status reg
        status_mask |= (1 << 1);
    } else if (cmd->resp_type == SD_RESP_R2){
        cmd_reg |= 1;
    } else if (cmd->resp_type != SD_RESP_NONE) {
        cmd_reg |= 2;
    }

    // R3 and R2 does not have a command index echo
    cmd_reg |= (1 << 4);
    if (cmd->resp_type == SD_RESP_R2 || cmd->resp_type == SD_RESP_R3) {
        cmd_reg &= ~(1 << 4);
    }

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
        
        if (cmd->cmd == 17 || cmd->cmd == 18) {
            // Use ADMA for command 17 and command 18
            u32 reg = mmc->HC1R & ~(0b11 << 3);
            mmc->HC1R = reg | (2 << 3);

            // DMA enable in the transfer register
            mode_reg |= (1 << 0);
        }

        // Set the timeout control register
        mmc->TCR = 0xE;

        // Set the block size register
        mmc->BSR = data->block_size;

        if (data->blocks) {
            mmc->BCR = data->blocks;
        }

        // Write the mode register 
        mmc->TMR = mode_reg;

        // Set up the DMA for command 17 and command 18
        if (cmd->cmd == 17 || cmd->cmd == 18) {
            
            // The user cannot request more than 16 blocks at a time
            if (data->blocks > 16) {
                panic("Too many blocks requested");
            }

            // Setup the ADMA table
            for (u32 i = 0; i < data->blocks; i++) {

                // We are not using interrupt at the moment
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

    // Writing to the command register trigger the SD command generation
    mmc->CR = cmd_reg;

    // Wait for the command to be done
    u32 status;
    timeout = 10000;
    do {
        status = mmc->NISTR;
    } while ((--timeout) && (status & status_mask) != status_mask);

    if (!timeout) {
        panic("Timeout during command completion");
    }

    print("Command complete\n");

    // Clear the status bits except the buffer read and write ready flags
    mmc->NISTR = status & ~(0b11 << 4);

    // If the timeout occurs the operating system will handle the exception. 
    // Otherwise the command is done so we can read the response
    if (cmd->resp_type == SD_RESP_R2) {
        // We have to read the entire 136 bit response
        for (u32 i = 0; i < 4; i++) {
            cmd->resp[i] = mmc->RR[i];
        }
    } else {
        cmd->resp[0] = mmc->RR[0];
    }

    // We currently use the ADMA for command 17 and command 18. So if we have a
    // data stage we wait for the ADMA to complete
    if (data) {
        timeout = 10000;
        do {
            status = mmc->NISTR;
        } while ((--timeout) && !(status & (1 << 1)));

        // The transfer complete flag is set
        if (!timeout) {
            panic("Timeout");
        }

        // Clear the flag
        mmc->NISTR = (1 << 1);
    }
}
