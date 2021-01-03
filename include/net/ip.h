
// Network IP module

#ifndef IP_H
#define IP_H

#include <citrus/types.h>
#include <net/netbuf.h>

#define MAC_BROADCAST (1 << 0)

// This is our IPv4 address
struct ip_struct {
    u32 our_ip;
    
    u32* dns_ip;
    u32 dns_cnt;

    u32* server_ip;
    u32 server_cnt;

    u32 gateway_ip;
    u32 subnet_mask;

    u32 lease_time;
};

i32 str_to_ipv4(const char* buf, u32* addr);

void ipv4_to_str(u32 addr, char* buf);

void ip_receive(struct netbuf* buf);

void ip_send(struct netbuf* buf, u32 src_ip, u32 dest_ip, u8 flags);

u32 get_src_ip(void);

void set_ip_addr(u32 ip);

void ip_print(struct ip_struct* ip);

void set_ip_struct(struct ip_struct* ip);

#endif
