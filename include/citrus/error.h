/// Copyright (C) strawberryhacker

#ifndef ERROR_H
#define ERROR_H

#include <citrus/types.h>

#define EDISK   1      // Disk error has occures
#define ENOFILE 2      // No file / dir
#define EEOF    3      // End of file marker
#define EEOCC   4      // End of cluster chain
#define EPATH   5      // Path error
#define ENOENT  6

#define ENOMEM  10     // No more memory
#define ENOPID  11     // No PID available

#define EBFONT  12     // Bad font
#define ECRC      13       // Bad font
#define ENSUPPORT 14

#define ENODMA 15  // No free DMA channel
#define ENODEV 16
#define ERETRY 17
#define EBADIP 18 // Bad IP address
#define ENET  19 // Gerneral network error

#endif
