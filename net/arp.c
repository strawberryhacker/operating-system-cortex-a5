
#include <net/arp.h>
#include <citrus/kmalloc.h>
#include <citrus/error.h>
#include <citrus/gmac.h>
#include <net/mac.h>
#include <citrus/thread.h>
#include <citrus/mem.h>
#include <citrus/panic.h>

static struct arp_table arp_table;

static i32 arp_thread(void* arg);

void arp_init(void)
{
    list_init(&arp_table.arp_list);

    create_kthread(arp_thread, 5000, "arp thread", NULL, SCHED_RT);
}

void arp_alloc_mapping(ipaddr_t ip)
{
    struct arp_entry* ent = kmalloc(sizeof(struct arp_entry));

    ent->ip = ip;
    ent->waiting = 1;

    // Add the new mapping into the ARP table
    list_add_first(&ent->node, &arp_table.arp_list);
}

// Maps an IPv4 address to a MAC address. The input MAC address has to be 6 bytes
void arp_add_new_mapping(ipaddr_t ip, const u8* mac)
{
    struct list_node* node;
    list_iterate(node, &arp_table.arp_list) {
        struct arp_entry* ent = list_get_entry(node, struct arp_entry, node);

        if (ent->ip == ip && ent->waiting == 1) {

            // Copy the mac address
            for (u32 i = 0; i < 6; i++) {
                ent->mac[i] = mac[i];
            }

            ent->waiting = 0;
        
            return;
        }
    }
    panic("Mapping does not exist\n");
}


i32 arp_search(ipaddr_t ip, u8* mac)
{
    struct list_node* node;
    list_iterate(node, &arp_table.arp_list) {

        struct arp_entry* ent = list_get_entry(node, struct arp_entry, node);

        // Check if the IPv4 matches
        if (ip == ent->ip && ent->waiting == 0) {
            for (u32 i = 0; i < 6; i++)
                mac[i] = ent->mac[i];
            
            return 0;
        }
    }
    return -ENOENT;
}

void arp_request(ipaddr_t dest_ip, ipaddr_t src_ip)
{
    // Get our MAC address
    const u8* src_mac = gmac_get_mac_addr();

    // Allocate a new mapping
    arp_alloc_mapping(dest_ip);

    // Request and initialize a netbuf
    struct netbuf* buf = alloc_netbuf();
    init_netbuf(buf);

    u8* ptr = buf->ptr;

    // Eternet type
    *ptr++ = 0x00;
    *ptr++ = 0x01;

    // IPv4 type
    *ptr++ = 0x08;
    *ptr++ = 0x00;

    // Address lengths for MAC and IPv4
    *ptr++ = 6;
    *ptr++ = 4;

    // ARP request
    *ptr++ = 0x00;
    *ptr++ = 0x01;

    // Copy in the source mac address
    for (u32 i = 0; i < 6; i++)
        *ptr++ = src_mac[i];
    
    // Get our IPv4 address
    *ptr++ = (src_ip >> 24 & 0xFF);
    *ptr++ = (src_ip >> 16 & 0xFF);
    *ptr++ = (src_ip >> 8  & 0xFF);
    *ptr++ = (src_ip >> 0  & 0xFF);

    // Dest MAC is ignored
    ptr += 6;

    // Copy the destination IPv4 address
    *ptr++ = (dest_ip >> 24 & 0xFF);
    *ptr++ = (dest_ip >> 16 & 0xFF);
    *ptr++ = (dest_ip >> 8  & 0xFF);
    *ptr++ = (dest_ip >> 0  & 0xFF);

    // Set the size of the MAC paylaod
    buf->frame_len = 28;

    // Send a ARP
    mac_broadcast(buf, 0x0806);
}

i32 arp_thread(void* arg)
{
    while (1) {
        struct netbuf* buf;

        i32 err = gmac_rec_raw(&buf);

        if (err) 
            continue;
        
        // We have a new packet
        mac_receive(buf);
    }
    return 0;
}

const u8 arp_resp[] = {0x00, 0x01, 0x08, 0x00, 0x06, 0x04, 0x00, 0x02};

void arp_receive(struct netbuf* buf)
{
    u8* ptr = buf->ptr;

    if (buf->frame_len < 28)
        return;
    
    // Check the first fields
    for (u32 i = 0; i < 8; i++) {
        if (*ptr++ != arp_resp[i])
            return;
    }

    // Check that the destination MAC address is ours
    u8* mac = ptr + 10;
    const u8* our_mac = gmac_get_mac_addr();

    for (u32 i = 0; i < 6; i++) {
        if (mac[i] != our_mac[i])
            return;
    }    

    ipaddr_t ip = read_be32(ptr + 6);

    arp_add_new_mapping(ip, ptr);
}
