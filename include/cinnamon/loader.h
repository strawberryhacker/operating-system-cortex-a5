/// Copyright (C) strawberryhacker

#ifndef LOADER_H
#define LOADER_H

#include <cinnamon/types.h>

#define MAX_DATA_SIZE 4096

enum loader_state {
    LOADER_IDLE,
    LOADER_CMD, 
    LOADER_SIZE,
    LOADER_DATA,
    LOADER_CRC,
    LOADER_END
};

struct packet {
    u8  padding;
    u8  cmd;
    u16 size;
    u8  data[MAX_DATA_SIZE];
    u8  crc;
};

void loader_init(void);

#endif
