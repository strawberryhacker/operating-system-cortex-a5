// Copyright (C) strawberryhacker

#include <citrus/sd.h>
#include <citrus/print.h>
#include <citrus/panic.h>
#include <citrus/mem.h>
#include <citrus/disk.h>
#include <citrus/kmalloc.h>
#include <citrus/interrupt.h>
#include <citrus/atomic.h>
#include <stddef.h>

// 178 card registers

#define SD_STATUS_ERROR (1 << 19)

// Issues the go to idle command
static u32 sd_go_to_idle(struct sd_card* sd)
{
    struct mmc_cmd* cmd = &sd->cmd;

    // Go to idle command
    cmd->cmd = 0;
    cmd->arg = 0;
    cmd->resp_type = SD_RESP_NONE;

    u32 status = sd->write(sd, cmd, NULL);

    if (!status) {
        panic("Go to idle command failed");
    }

    return 1;
}

// This sends the 
static u32 sd_send_interface_condition(struct sd_card* sd)
{
    struct mmc_cmd* cmd = &sd->cmd;

    // Get the operating conditions from the host driver. This assumes that the
    // host driver can operate between 2.7 - 3.6 V
    u32 arg = (0b0001 << 8) | 0b10101010;

    // Send IF condition
    cmd->cmd = 8;
    cmd->arg = arg | 0b10101010;
    cmd->resp_type = SD_RESP_R7;

    u32 status = sd->write(sd, cmd, NULL);

    if (!status) {
        panic("Send interface condition failed");
    }

    // Check the response
    if ((cmd->resp[0] & arg) != arg) {
        
        // Wrong interface condition reeived. This does not mean that the 
        // card is unusable, but indicated a possible SD version 1.00 card
        sd->version = SD_VERSION_1_XX;
    } else {
        sd->version = SD_VERSION_2_00;
    }

    return 1;
}

// Send CMD55 which indicates that the next command is an application spesific
// command
static u32 sd_cmd55(struct sd_card* sd)
{
    // The command 55 should not use the sd card cmd structure since it will be
    // used by the application commands
    struct mmc_cmd cmd;

    cmd.cmd = 55;
    cmd.arg = sd->card_addr;
    cmd.resp_type = SD_RESP_R1;

    u32 status = sd->write(sd, &cmd, NULL);

    // Check for error
    if (!status) {
        panic("Command 55 returned an error");
    }

    return 1;
}

// Send operating conditions. This is issued by the host after the completion
// of send interface condition. This will send the host capacity support and
// the card will return the OCR register
static u32 sd_send_op_cond(struct sd_card* sd)
{
    u32 arg = 0;

    // The receiving of command 8 expands the functions of this command
    if (sd->version == SD_VERSION_2_00) {

        // The card responded to command 8 which expands the HCS bit. This host
        // interface supports high capacity SD cards
        arg |= (1 << 30);
    }

    // Set the host voltage window support
    u32 cap = sd->mmc->CA0R;

    // 3.3V support
    if (cap & (1 << 24)) {
        arg |= (0b11 << 20);
    }

    // 3.0V support
    if (cap & (1 << 25)) {
        arg |= (0b11 << 17);
    }

    // Make the command
    struct mmc_cmd* cmd = &sd->cmd;

    cmd->cmd = 41;
    cmd->arg = arg;
    cmd->resp_type = SD_RESP_R3;

    // The host will repeatadly send the ACMD41 untill the card is not busy. 
    // This should happend whithin 1 second 
    u32 timeout = 1000;
    do {
        if (!sd_cmd55(sd)) {
            return 0;
        }

        u32 status = sd->write(sd, cmd, NULL);
        if (!status) {
            panic("ACMD41 error");
        }

        // Check the busy signaling from the card
        if (cmd->resp[0] & (1 << 31)) {

            // Bit number 31 indicates if the card supports high capacity
            if (cmd->resp[0] & (1 << 30)) {
                sd->sdhc = 1;
            } else {
                sd->sdhc = 0;
            }

            return 1;
        }

    } while (--timeout);

    panic("Timeout on ACMD41");

    return 0;
}

// Gets the CID register from the card. After a successful reception of this 
// command the card will enter the identification stage
static u32 sd_send_all_cid(struct sd_card* sd)
{
    struct mmc_cmd* cmd = &sd->cmd;

    cmd->cmd = 2;
    cmd->arg = 0;
    cmd->resp_type = SD_RESP_R2;

    u32 status = sd->write(sd, cmd, NULL);

    const char* src = (const char *)cmd->resp;
    for (u32 i = 0; i < 6; i++) {
        sd->cid_name[i] = src[12 - i];
    }
    sd->cid_name[6] = 0;

    // The card is now in identification state

    return 1;
}

// Send realative address command. The host sends command 3 in order to request
// the card to publish a RCA number. The host then sets the card relative 
// address
static u32 sd_send_relative_addr(struct sd_card* sd)
{
    struct mmc_cmd* cmd = &sd->cmd;

    cmd->cmd = 3;
    cmd->arg = 0;
    cmd->resp_type = SD_RESP_R6;

    u32 status = sd->write(sd, cmd, NULL);

    if (!status) {
        panic("Error receiving RCA");
    }

    // We are only operating one SD card per slot so we are happy with the first
    // RCA address
    sd->card_addr = 0xFFFF0000 & cmd->resp[0];

    // The card is at this point entering the stand-by state

    return 1;
}

// Read the card spesific data (the CSD register). The card will remain inn the
// same state after the reception of this command
static u32 sd_get_csd(struct sd_card* sd)
{
    struct mmc_cmd* cmd = &sd->cmd;

    cmd->cmd = 9;
    cmd->arg = sd->card_addr;
    cmd->resp_type = SD_RESP_R2;

    u32 status = sd->write(sd, cmd, NULL);

    if (!status) {
        panic("Error receiving RCA");
    }

    u8* csd = (u8 *)cmd->resp;

    // Calculate the card size from the CSD register
    if (((csd[14] >> 6) & 0b11) == 1) {

        // Version 2.0 extended capacity SD card
        u32 c_size = ((csd[7] & 0x3F) << 16) | (csd[6] << 8) | csd[5];
        sd->size_kb = (c_size + 1) * 512;

    } else {
        u32 c_size      = ((csd[7] >> 6) & 0b11) | (csd[8] << 2) | 
                          ((csd[9] & 0b11) << 10);
        u32 c_size_mult = (((csd[6] & 0b11) << 1) | ((csd[5] >> 7) & 0b1));
        u32 read_bl_len = (csd[10] & 0b1111);

        sd->size_kb = ((c_size + 1) * (1 << (c_size_mult + 2)) *
                        (1 << read_bl_len)) / 1000;
    }

    return 1;
}

// Toggles a card between the transfer mode and the stand-by mode
static u32 sd_select_deselect(struct sd_card* sd)
{
    struct mmc_cmd* cmd = &sd->cmd;

    cmd->cmd = 7;
    cmd->arg = sd->card_addr;
    cmd->resp_type = SD_RESP_R1b;

    // Send the command
    u32 status = sd->write(sd, cmd, NULL);
    
    if (!status) {
        panic("Select deselect error");
    }

    return 1;
}

// Inverts the byte order in a word
static void sd_invert_byte_order(u32* data)
{
    u32 reg = *data;
    u32 res = 0;

    for (u32 i = 0; i < 4; i++) {
        res <<= 8;
        res |= reg & 0xFF;
        reg >>= 8;
    }
    *data = res;
}

// Get the card CSR register and check whether it supports 4 bit bus mode
static u32 sd_get_scr(struct sd_card* sd)
{
    struct mmc_cmd* cmd = &sd->cmd;
    struct mmc_data* data = &sd->data;

    cmd->cmd = 51;
    cmd->arg = 0;
    cmd->resp_type = SD_RESP_R1;

    u32 data_buff[2];
    data->data = (u8 *)data_buff;
    data->block_size = 8;
    data->blocks = 1;
    data->dir = 0;

    // Send the command
    sd_cmd55(sd);
    u32 status = sd->write(sd, cmd, data);

    if (!status) {
        panic("ACMD51 fails");
    }

    sd_invert_byte_order(&data_buff[0]);
    sd_invert_byte_order(&data_buff[1]);

    // Check if we have 4 bit bus support
    u8 bus_width = (data_buff[0] >> 16) & 0xF;
    if (bus_width & (1 << 2)) {
        sd->bus4 = 1;
    }

    u8 version = (data_buff[0] >> 24) & 0xF;
    if (version) {
        sd->not_v1_0 = 1;
    }
    return 1;
}

// Switch the bus mode
static u32 sd_set_bus_width(u32 bus_width, struct sd_card* sd) {
    assert(bus_width == 1 || bus_width == 4);

    struct mmc_cmd* cmd = &sd->cmd;

    cmd->cmd = 6;
    cmd->arg = (bus_width == 4) ? 0b10 : 0b00;
    cmd->resp_type = SD_RESP_R1;

    // Send the command
    sd_cmd55(sd);
    u32 status = sd->write(sd, cmd , NULL);

    if (!status) {
        panic("Cant set bus width");
    }

    status = cmd->resp[0];

    if (status & 0xFFF80000) {
        panic("Error");
    }

    return 1;
}

// Checks high speed support. This requires the card to have a higher version
// than 1.0. Otherwise the CMD6 is not supported
static u32 sd_check_high_speed(struct sd_card* sd)
{
    struct mmc_cmd* cmd = &sd->cmd;
    struct mmc_data* data = &sd->data;

    u8 buffer[64];
    mem_set(buffer, 0, 64);

    cmd->cmd = 6;
    cmd->arg = (0 << 31) | (0xF << 12) | (0xF << 8) | (0xF << 4) | (0 << 0);
    cmd->resp_type = SD_RESP_R1;

    data->data = buffer;
    data->block_size = 64;
    data->blocks = 1;
    data->dir = 0;

    // Send the command
    u32 status = sd->write(sd, cmd, data);

    if (!status) {
        panic("Error");
    }

    // Check the command response
    status = cmd->resp[0];
    if (status & (1 << 7)) {
        return 0;
    }
}

// Initializes the SD card structure. This must be done before starting the 
// enumeration
static void sd_init_struct(struct sd_card* sd)
{
    sd->card_addr = 0;
}

// Checks if a card is ready for receive new data
static u32 sd_check_chard_ready(struct sd_card* sd)
{
    struct mmc_cmd* cmd = &sd->cmd;

    // Fill in the arguments
    cmd->cmd = 13;
    cmd->arg = sd->card_addr;
    cmd->resp_type = SD_RESP_R1;

    u32 timeout = 20000;
    u32 status;
    do {
        status = sd->write(sd, cmd, NULL);

        if (!status) {
            panic("Error");
        }
        status = cmd->resp[0];

        if (--timeout == 0) {
            return 0;
        }
    } while (!(status & (1 << 8)));

    return 1;
}

// Reads from the SD card 
static inline u32 sd_read(struct sd_card* sd, u32 sect, u32 cnt, u8* buffer)
{
    assert(buffer);

    // TODO check overflow
    for (u32 i = 0; i < cnt; i++) {

        if (!sd_check_chard_ready(sd)) {
            panic("Card is not ready");
        }

        struct mmc_cmd* cmd = &sd->cmd;
        struct mmc_data* data = &sd->data;

        // Fill in the cmd field
        cmd->cmd = 17;
        cmd->resp_type = SD_RESP_R1;

        data->block_size = 512;
        data->blocks = 1;
        data->dir = 0;

        // Fill in the argument
        if (sd->sdhc) {
            cmd->arg = sect + i;
        } else {
            cmd->arg = (sect + i) * 512;
        }

        // Update the buffer pointer
        data->data = buffer + 512 * i;

        // Execute the command
        u32 status = sd->write(sd, cmd, data);

        if (!status) {
            panic("Error");   
        }
        if (cmd->resp[0] & 0xFFF80000) {
            panic("Write error");
        }
    }
    return 1;
}

u32 sd_disk_read(const struct disk* disk, u32 sect, u32 cnt, u8* data)
{
    struct sd_card* sd = (struct sd_card *)disk->priv;
    
    return sd_read(sd, sect, cnt, data);
}

// Created a phyiscal disk from the specified SD card
struct disk* sd_create_disk(struct sd_card* sd)
{
    struct disk* disk = kzmalloc(sizeof(struct disk));

    disk->priv = sd;
    disk->read = sd_disk_read;

    return disk;
}

// This thread is launced by the scheduler when a new SD has been inserted. 
// This will initialize and enumerate the SD card such that the SD card can 
// be accessed by the file system driver. After the initialization it will 
// make a disk object and mount the disk
i32 sd_init_thread(void* sd_card)
{
    // This thread is given a new SD card struct in the parameter list
    struct sd_card* card = (struct sd_card *)sd_card;
    
    if (!card) return 0;

    sd_init_struct(card);

    // Configure the MMC device for intialization
    card->set_frequency(card, 400000);
    card->set_bus_width(card, 1);

    // -----------------------------------------

    // Start of the enumeration process
    sd_go_to_idle(card);
    sd_send_interface_condition(card);
    sd_send_op_cond(card);

    // After this point the SDHC support and the SD version is fully determined
    sd_send_all_cid(card);
    sd_send_relative_addr(card);
    sd_get_csd(card);
    sd_select_deselect(card);

    // The addressed card is now in the transfer state
    sd_get_scr(card);

    // Try to change to 4 bit bus
    if (card->bus4) {
        sd_set_bus_width(4, card);
        card->set_bus_width(card, 4);
    }

    card->set_frequency(card, 25000000);

    struct disk* disk = sd_create_disk(card);

    // Add the new disk to the kernel
    disk_add(disk, DISK_SD);

    return 1;   
}
