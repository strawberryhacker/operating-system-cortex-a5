/// Copyright (C) strawberryhacker

#include <cinnamon/sd.h>
#include <cinnamon/print.h>
#include <cinnamon/panic.h>
#include <stddef.h>

/// Issues the go to idle command
static void sd_go_to_idle(struct sd* sd)
{
    struct mmc_cmd* cmd = &sd->cmd;

    // Go to idle command
    cmd->cmd = 0;
    cmd->arg = 0;
    cmd->resp_type = SD_RESP_NONE;

    u32 status = sd->write_cmd(sd, cmd, NULL);

    if (!status) {
        panic("Go to idle command failed");
    }
}

static void sd_send_interface_condition(struct sd* sd)
{
    struct mmc_cmd* cmd = &sd->cmd;

    // Get the operating conditions from the host driver. This assumes that the
    // host driver can operate between 2.7 - 3.6 V
    u32 arg = 0b0001 | 0b10101010;

    // Send IF condition
    cmd->cmd = 8;
    cmd->arg = arg | 0b10101010;
    cmd->resp_type = SD_RESP_R7;

    u32 status = sd->write_cmd(sd, cmd, NULL);

    if (!status) {
        panic("Send interface condition failed");
    }

    // Check the response
    if ((cmd->resp[0] & arg) != arg) {
        panic("Wrong interface condition received");
    }

    print("Success\n");
}

/// This thread is launced by the scheduler when a new SD has been inserted. 
/// This will initialize and enumerate the SD card such that the SD card can 
/// be accessed by the file system driver
u32 sd_init_thread(void* sd)
{
    // This thread is given a new SD card struct in the parameter list
    struct sd* card = (struct sd *)sd;

    if (!card) return 0;

    sd_go_to_idle(card);
    sd_send_interface_condition(card);

    return 1;
}
