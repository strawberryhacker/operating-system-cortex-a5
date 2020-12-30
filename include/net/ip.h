
// Network IP module

#ifndef IP_H
#define IP_H

#include <citrus/types.h>

typedef u32 ipaddr_t;

i32 str_to_ipaddr(const char* buf, ipaddr_t* addr);

void ipaddr_to_str(ipaddr_t addr, char* buf);

#endif
