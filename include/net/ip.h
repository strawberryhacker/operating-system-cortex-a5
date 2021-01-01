
// Network IP module

#ifndef IP_H
#define IP_H

#include <citrus/types.h>
#include <net/netbuf.h>

typedef u32 ipaddr_t;

i32 str_to_ipv4(const char* buf, ipaddr_t* addr);

void ipv4_to_str(ipaddr_t addr, char* buf);

void ip_receive(struct netbuf* buf);

#endif