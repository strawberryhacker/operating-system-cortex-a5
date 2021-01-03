// MAC layer

#ifndef MAC_H
#define MAC_H

#include <citrus/types.h>
#include <net/ip.h>
#include <net/netbuf.h>

void mac_init(void);

void mac_receive(void);

void mac_send(struct netbuf* buf, u32 dest_ip, u32 src_ip, u16 type, 
    u8 broadcast);

void mac_broadcast(struct netbuf* buf, u16 type);

void mac_unqueue(u32 ip);

void mac_copy(const void* src, void* dest);

#endif
