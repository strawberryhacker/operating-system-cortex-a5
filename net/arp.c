#include <net/arp.h>
#include <net/ip.h>
#include <net/netbuf.h>
#include <citrus/gmac.h>
#include <citrus/mem.h>

#include <citrus/error.h>
#include <citrus/kmalloc.h>
#include <citrus/print.h>

// This is a queue of netbuffers
struct list_node arp_queue;

// Global ARP table
struct arp_table arp_table;

void arp_init(void)
{  
    // Initilaize the ARP table
    list_init(&arp_table.arp_list);
    arp_table.size = 0;

    // Initialize the ARP queue
    list_init(&arp_queue);
}

static void arp_add_map(const u8* mac, ipaddr_t ip)
{
    struct arp_entry* entry = kmalloc(sizeof(struct arp_entry));

    for (u32 i = 0; i < 6; i++)
        entry->mac[i] = mac[i];
    
    entry->ip = ip;

    // Link it in the ARP table
    list_add_last(&entry->node, &arp_table.arp_list);
}

// Searches thought the ARP table and tries to find the MAC address which 
// matches the given IP address.
// 
// The MAC address buffer must be at least 6 bytes long
static i32 arp_get_mac(u8* mac, ipaddr_t ip)
{
    struct list_node* node;
    list_iterate(node, &arp_table.arp_list) {
        
        // Get the ARP entry from the list
        struct arp_entry* e = list_get_entry(node, struct arp_entry, node);

        if (e->ip == ip) {
            for (u32 i = 0; i < 6; i++) {
                mac[i] = e->mac[i];
            }
            return 0;
        }
    }
    return -ENOENT;
}

void arp_handle(struct netbuf* buf)
{
    // Strip the MAC header ourself
    buf->header += 14;
    buf->frame_len -= 14;

    // We have to use Ethernet
    if (read_be16(buf->header) != 1)
        return;
    buf->header += 2;
    
    // Check if the protocol if IPv4
    if (read_be16(buf->header) != 0x0800) 
        return;
    buf->header += 2;

    // Check the MAC addr length
    if (*buf->header++ != 6)
        return;
    
    // IPv4 lenght
    if (*buf->header++ != 4)
        return;
    
    if (read_be16(buf->header) != 2)
        return;
    buf->header += 2;
    
    ipaddr_t ip_addr = read_be32(buf->header + 6);
    arp_add_map(buf->header, ip_addr);

    // Search the ARP queue and flush all packets which has gotten and ARP resp
    struct list_node* node;
    list_iterate(node, &arp_queue) {
        struct netbuf* new_netbuf = list_get_entry(node, struct netbuf, node);

        // The destination IP address matches
        if (new_netbuf->ip == ip_addr) {

            // Fill in the new MAC address and send the frame
            mem_copy(buf->header, new_netbuf->header, 6);

            gmac_send_raw(new_netbuf->header, new_netbuf->frame_len);
        }
    }
}

static u8 arp_buffer[1538];

static void arp_request(ipaddr_t src_ip, ipaddr_t dest_ip)
{
    const u8* src_mac = gmac_get_mac_addr();

    u8* arp = arp_buffer;

    // Filling in the destination broadcast mac address
    for (u32 i = 0; i < 6; i++)
        *arp++ = 0xFF;
    
    for (u32 i = 0; i < 6; i++)
        *arp++ = src_mac[i];
    
    // ARP Type 
    *arp++ = 0x08;
    *arp++ = 0x06;

    // Hardware type
    arp++;
    *arp++ = 1;

    // IPv4 protocol
    *arp++ = 0x08;
    *arp++ = 0x00;

    *arp++ = 6; // MAC length
    *arp++ = 4; // IPv4 length6 

    arp++;
    *arp++ = 1;
        
    for (u32 i = 0; i < 6; i++)
        *arp++ = src_mac[i];
    
    // Source IP address
    u8* tmp = (u8 *)&src_ip + 3;
    for (u32 i = 0; i < 4; i++) {
        *arp++ = *tmp--;
    }

    // Dest MAC ignored
    arp += 6;

    // Source IP address
    tmp = (u8 *)&dest_ip + 3;
    for (u32 i = 0; i < 4; i++) {
        *arp++ = *tmp--;
    }

    gmac_send_raw(arp_buffer, 42);
}

// Wraps the packet in a mac header with the requested source and destination
// ip address and the typearp_queue; field. This will querry the diver for the MAC
// address
i32 mac_out(struct netbuf* buf, ipaddr_t src_ip, ipaddr_t dest_ip, u16 type)
{
    buf->ip = dest_ip;
    const u8* src_mac = gmac_get_mac_addr();

    u8* header = buf->header;

    *--header = type & 0xFF;
    *--header = (type >> 8) & 0xFF;

    // Fill in the source mac address
    for (u32 i = 0; i < 6; i++) {
        *--header = src_mac[5 - i];
    }

    // Check the destination MAC address
    u8 dest_mac[6];

    i32 err = arp_get_mac(dest_mac, dest_ip);
    
    // We have to do a MAC request
    if (err) {

        print("ARP table serach failed\n");

        // Update the header field
        buf->header = header - 6;

        arp_request(src_ip, dest_ip);

        return 0;
    }

    for (u32 i = 0; i < 6; i++)
        *--header = dest_mac[5 - i];
    
    // Update the new address of the the header
    buf->header = header;
    buf->frame_len += 14;

    gmac_send_raw(buf->header, buf->frame_len);

    print("OK\n");

    return 0;
}
