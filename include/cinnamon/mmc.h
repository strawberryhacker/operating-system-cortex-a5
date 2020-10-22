/// Copyright (C) strawberryhacker

#ifndef MMC_H
#define MMC_H

#include <cinnamon/types.h>
#include <regmap.h>


struct mmc_data {
    u8* data;
    u32 dir;
    u32 blocks;
    u32 block_size;
};

struct mmc_cmd {
    u32 cmd;
    u32 arg;
    u32 resp_type;
    u32 resp[4];
};

/// This driver implements the SD host controller driver V3.0

struct sd {

    // Private data for the SD card
    struct mmc_reg* mmc;

    u32 size_kb;

    // Functions for accessing this SD card
    u32 (*init)(struct sd* sd);
    u32 (*write_cmd)(struct sd* sd, struct mmc_cmd* cmd, struct mmc_data* data);
    
};


/// SD card responses
#define SD_RESP_NONE 0
#define SD_RESP_R1   1
#define SD_RESP_R1b  2
#define SD_RESP_R2   3
#define SD_RESP_R3   4
#define SD_RESP_R6   5
#define SD_RESP_R7   6

void mmc_init(struct mmc_reg* mmc);

void sd_card_init(void);

void sd_protocol_init(struct mmc_reg* mmc);

#endif
