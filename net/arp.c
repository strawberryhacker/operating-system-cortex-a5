
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

i32 arp_alloc_mapping(u32 ip)
{
    // Search for an allready existing mapping
    struct list_node* node;
    list_iterate(node, &arp_table.arp_list) {
        struct arp_entry* ent = list_get_entry(node, struct arp_entry, node);

        if (ent->ip == ip && ent->waiting == 0) {
            panic("ARP error");
        } else if (ent->ip == ip) {
            return -ENOENT;
        }
    }

    struct arp_entry* ent = kmalloc(sizeof(struct arp_entry));

    ent->ip = ip;
    ent->waiting = 1;

    // Add the new mapping into the ARP table
    list_add_first(&ent->node, &arp_table.arp_list);

    return 0;
}

// Maps an IPv4 address to a MAC address. The input MAC address has to be 6 bytes
void arp_add_new_mapping(u32 ip, const u8* mac)
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


i32 arp_search(u32 ip, u8* mac)
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

void arp_request(u32 dest_ip, u32 src_ip)
{
    // Get our MAC address
    const u8* src_mac = gmac_get_mac_addr();

    // Allocate a new mapping
    i32 err = arp_alloc_mapping(dest_ip);
    if (err) {
        print("ARP waiting\n");
    }

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

    char ip_name[16];
    ip_name[15] = 0;
    ipv4_to_str(dest_ip, ip_name);
    print("ARP request to IP %s\n", ip_name);

    // Send a ARP
    mac_broadcast(buf, 0x0806);
}

i32 arp_thread(void* arg)
{
    while (1) {        
        // We have a new packet
        //mac_receive();
    }
    return 0;
}

struct __attribute__((packed)) arp_header {
    u16 htype;
    u16 ptype;
    u8 hlen;
    u8 plen;
    u16 oper;
    u8 sha[6];
    u32 spa;
    u8 tha[6];
    u32 tpa;    
};

#define ARP_REQUEST 1
#define ARP_REPLY 2

// This will handle a ARP req and send a ARP reply back
void arp_handle_req(struct netbuf* buf)
{
    // Get a netbuf packet
    struct netbuf* rep_buf = alloc_netbuf();

    struct arp_header* dest_hdr = (struct arp_header *)rep_buf->ptr;

    store_be16(0x0001, &dest_hdr->htype);  // Ethernet
    store_be16(0x0800, &dest_hdr->ptype);  // IPv4
    dest_hdr->hlen = 6;                    // MAC length
    dest_hdr->plen = 4;                    // IPv4 length

    // This is a reply
    store_be16(ARP_REPLY, &dest_hdr->oper);

    // Our MAC address
    mac_copy(gmac_get_mac_addr(), dest_hdr->sha);

    // Our IPv4 address
    store_be32(get_src_ip(), &dest_hdr->spa);

    struct arp_header* src_hdr = (struct arp_header *)buf->ptr;

    // Dest MAC
    mac_copy(src_hdr->sha, dest_hdr->tha);

    u32 dest_ip = read_be32(&src_hdr->spa);

    // Dest IPv4
    store_be32(dest_ip, &dest_hdr->tpa);

    // Send the raw MAC packet
    mac_send(rep_buf, dest_ip, get_src_ip(), 0x0806, 0);
}

void arp_handle_reply(struct netbuf* buf)
{
    struct arp_header* hdr = (struct arp_header *)buf->ptr;

    // Check that the destination MAC address is ours

    const u8* our_mac = gmac_get_mac_addr();
    for (u32 i = 0; i < 6; i++) {
        if (hdr->tha[i] != our_mac[i])
            return;
    }

    // Ignore the IPv4 field

    // Get the senders IP address
    u32 ip = read_be32(&hdr->spa);

    // Add the new mapping between the IPv4 and the MAC
    arp_add_new_mapping(ip, hdr->sha);
    mac_unqueue(ip);
}

void arp_receive(struct netbuf* buf)
{
    u8* ptr = buf->ptr;

    if (buf->frame_len < 28)
        return;

    struct arp_header* hdr = (struct arp_header *)buf->ptr; 

    // Check for ethernet
    if (read_be16(&hdr->htype) != 1)
        return;
    
    // Check for IPv4
    if (read_be16(&hdr->ptype) != 0x0800)
        return;
    
    // MAC address is 6 bytes
    if (hdr->hlen != 6)
        return;

    // IPv4 address is 4 bytes
    if (hdr->plen != 4)
        return;
    
    // Check if this is an ARP replay or an ARP request
    if (read_be16(&hdr->oper) == ARP_REPLY) {
        arp_handle_reply(buf);
    } else if (read_be16(&hdr->oper) == ARP_REQUEST) {
        arp_handle_req(buf);
    }
}
