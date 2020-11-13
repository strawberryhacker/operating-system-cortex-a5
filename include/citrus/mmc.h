/// Copyright (C) strawberryhacker

#ifndef MMC_H
#define MMC_H

#include <citrus/types.h>
#include <citrus/regmap.h>

enum sd_version {
    SD_VERSION_1_XX,
    SD_VERSION_2_00
};

struct mmc_data {
    u8* data;

    // 1 for out 0 for in
    u32 dir;
    u32 blocks;
    u32 block_size;
};

// CMD55 CMD2

struct mmc_cmd {
    u32 cmd;
    u32 arg;
    u32 resp_type;
    u32 resp[4];
};

/// This driver implements the SD host controller driver V3.0

struct sd_card {

    // Private register interface for the sd card
    struct mmc_reg* mmc;

    enum sd_version version;
    char cid_name[7];

    // Card address including the stuff bits [0..15]
    u32 card_addr;
    
    u32 bus4       : 1;
    u32 not_v1_0   : 1;
    u32 sdhc       : 1;
    u32 high_speed : 1;

    u32 size_kb;

    // Make a command and a data structure
    struct mmc_data data;
    struct mmc_cmd cmd;

    // Functions for accessing this SD card
    u32 (*write)(struct sd_card* sd, struct mmc_cmd* cmd, struct mmc_data* data);
    void (*set_bus_width)(struct sd_card* sd, u32 bus_width);
    void (*set_high_speed)(struct sd_card* sd, u32 high_speed);
    void (*set_frequency)(struct sd_card* sd, u32 frequency);
};


/// SD card responses
#define SD_RESP_NONE 0
#define SD_RESP_R1   1
#define SD_RESP_R1b  2
#define SD_RESP_R2   3
#define SD_RESP_R3   4
#define SD_RESP_R6   5
#define SD_RESP_R7   6

void mmc_init(void);

void sd_card_init(void);

u32 mmc_send_command(struct sd_card* sd, struct mmc_cmd* cmd, struct mmc_data* data);

#endif
