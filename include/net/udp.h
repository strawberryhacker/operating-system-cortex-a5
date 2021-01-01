#ifndef UDP_H
#define UDP_H

#include <citrus/types.h>
#include <net/netbuf.h>

void udp_receive(struct netbuf* buf);

#endif