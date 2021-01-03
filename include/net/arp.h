// ARP module

#ifndef ARP_H
#define ARP_H

#include <citrus/types.h>
#include <citrus/list.h>
#include <net/ip.h>
#include <net/netbuf.h>

struct arp_entry {
    struct list_node node;

    u32 ip;
    u8 mac[6];

    // Set to one if the stack is waiting for an ARP response
    u8 waiting;
};

struct arp_table {
    struct list_node arp_list;

};

void arp_init(void);

i32 arp_alloc_mapping(u32 ip);

void arp_add_new_mapping(u32 ip, const u8* mac);

i32 arp_search(u32 ip, u8* mac);

void arp_request(u32 dest_ip, u32 src_ip);

void arp_receive(struct netbuf* buf);

#endif