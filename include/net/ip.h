
// Network IP module

#ifndef IP_H
#define IP_H

#include <citrus/types.h>
#include <net/netbuf.h>

typedef u32 ipaddr_t;

#define MAC_BROADCAST (1 << 0)


i32 str_to_ipv4(const char* buf, ipaddr_t* addr);

void ipv4_to_str(ipaddr_t addr, char* buf);

void ip_receive(struct netbuf* buf);

void ip_send(struct netbuf* buf, u32 src_ip, u32 dest_ip, u8 flags);

u32 get_src_ip(void);

void set_ip_addr(u32 ip);

#endif
