#ifndef NETBUF_H
#define NETBUF_H

#include <citrus/types.h>
#include <citrus/list.h>

struct netbuf {

    struct list_node node;

    // Virtual start address of the buffer
    u8* buf;
    u32 buf_len;

    // A moving pointer set by each layer to the start of that layers header
    u8* header;

    // This is increased as the frame goes through the stack
    u32 frame_len;

    // IPv4 dest address for used in the ARP queue
    ipaddr_t ip;
};

#endif