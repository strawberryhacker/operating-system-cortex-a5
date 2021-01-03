#ifndef UDP_H
#define UDP_H

#include <citrus/types.h>
#include <net/netbuf.h>
#include <citrus/list.h>

// This is the main UDP module
struct udp {

    struct list_node ports;
};

// This is allocated for each bound port. Incoming frames from any application
// which is targeting this port will be placed here
struct udp_port {

    // This should be linked in the UDP module
    struct list_node node;
    struct list_node packets;

    u16 port;
};

void udp_init(void);

struct netbuf* get_netbuf_from_port(u16 port);

void add_netbuf_to_port(struct netbuf* buf, u16 port);

void udp_handle(struct netbuf* buf);

void udp_listen(u16 port);

struct netbuf* udp_rec(u16 port);

void udp_send(struct netbuf* buf, u32 ip, u16 port, u8 flags);

void udp_send_from(struct netbuf* buf, u32 dest_ip, u32 src_ip, u16 src_port, u16 dest_port, u8 flags);

void udp_get_free_port(u16* port);

#endif