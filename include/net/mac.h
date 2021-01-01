// MAC layer

#ifndef MAC_H
#define MAC_H

#include <citrus/types.h>
#include <net/ip.h>
#include <net/netbuf.h>

void mac_receive(struct netbuf* buf);

void mac_send(struct netbuf* buf, ipaddr_t dest_ip, ipaddr_t src_ip, u16 type);

void mac_broadcast(struct netbuf* buf, u16 type);

#endif
