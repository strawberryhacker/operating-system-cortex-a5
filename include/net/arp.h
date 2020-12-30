// ARP module 

#ifndef MAC_H
#define MAC_H

#include <citrus/types.h>
#include <citrus/list.h>
#include <net/ip.h>
#include <net/netbuf.h>

// Main ARP table
struct arp_table {
    struct list_node arp_list;
    u32 size;
};

// This descriptbes one entry in the ARP table. This maps an IP address to a 
// MAC address
struct arp_entry {

    struct list_node node;

    ipaddr_t ip;
    u8 mac[6];
};

struct mac_header {
    u8 dest_mac[6];
    u8 src_mac[6];
    u16 type;
};

void arp_init(void);

i32 mac_out(struct netbuf* buf, ipaddr_t src_ip, ipaddr_t dest_ip, u16 type);

void arp_handle(struct netbuf* buf);

#endif

