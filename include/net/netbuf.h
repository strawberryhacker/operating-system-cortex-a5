// Implementation of the netbuf packet pool

#ifndef NETBUF_H
#define NETBUF_H

#include <citrus/types.h>
#include <citrus/list.h>

#define NETBUF_LENGTH 1536
#define MAX_HEADER_SIZE 74

struct netbuf {
    u8 reserved0[32];

    // Virtual start address of the buffer
    u8 buf[NETBUF_LENGTH];

    u8 reserved1[32];

    // This is for the netbuf pool free list. And for queueing in the network
    // stack
    struct list_node node;

    // A moving pointer set by each layer to the start of that layers header
    u8* ptr;

    // This is increased as the frame goes through the stack
    u32 frame_len;

    // This is used for ARP response. When a ARP response is received this 
    // field is used for sending the queued packets
    u32 ip;
};

void netbuf_init(void);

struct netbuf* alloc_netbuf(void);

void free_netbuf(struct netbuf* buf);

static inline void init_netbuf(struct netbuf* buf)
{
    buf->ptr = buf->buf + MAX_HEADER_SIZE;
}

struct list_node* get_netbuf_list(void);

#endif
