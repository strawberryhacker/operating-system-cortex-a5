#include <net/mac.h>
#include <net/netbuf.h>
#include <citrus/gmac.h>
#include <net/arp.h>
#include <citrus/panic.h>
#include <citrus/mem.h>

// Packets in which the MAC address cant be reolved is placed into this list
static struct list_node arp_queue;

void mac_init(void)
{
    list_init(&arp_queue);
}

void mac_unqueue(u32 ip)
{
    struct list_node* node;

    list_iterate(node, &arp_queue) {
        struct netbuf* buf = list_get_entry(node, struct netbuf, node);

        if (buf->ip == ip) {

            u8 mac[6];

            // Look up the MAC address
            i32 err = arp_search(ip, mac);
            if (err)
                panic("MAC not found");
            
            // Fill in the MAC without touching pointers
            for (u32 i = 0; i < 6; i++) {
                buf->ptr[i] = 0x44; // MAC[i]
            }
            gmac_send_raw(buf);
        }
    }
}

void mac_receive(void)
{
    struct netbuf* buf;

    i32 err = gmac_rec_raw(&buf);

    if (err)
        return;

    // Skip the source and destination MAC
    buf->ptr += 12;

    // Get the MAC type field
    u16 type = read_be16(buf->ptr);
    buf->ptr += 2;

    // Shrink with the MAC header length
    buf->frame_len -= 14;

    if (type == 0x0806) {

        // This will both handle reply and requests
        arp_receive(buf);

    } else if (type == 0x0800) {
        ip_receive(buf);
    }
}

// Fill in the MAC frame and get the dest MAC address form the ARP module
void mac_send(struct netbuf* buf, u32 dest_ip, u32 src_ip, u16 type, 
    u8 broadcast)
{
    // Check that the buffers are given in a reight format
    assert(buf->buf + 14 <= buf->ptr);

    // FF:FF:FF:FF:FF:FF
    const u8* src_mac = gmac_get_mac_addr();

    // Fill in the type field
    *--buf->ptr = type & 0xFF;
    *--buf->ptr = (type >> 8) & 0xFF;

    // Fill in the source mac address
    for (u32 i = 0; i < 6; i++)
        *--buf->ptr = src_mac[5 - i];
    
    // Update the size of the frame
    buf->frame_len += 14;

    // Save the destination IP address
    buf->ip = dest_ip;

    // If this is a broadcast MAC message
    if (broadcast) {

        // Fill in the broadcast mac address
        for (u32 i = 0; i < 6; i++)
            *--buf->ptr = 0xFF;   

        gmac_send_raw(buf);

        return;
    }

    // Get the destination MAC address from the ARP table
    u8 dest_mac[6];
    i32 err = arp_search(dest_ip, dest_mac);

    if (err) {
        buf->ptr -= 6;
        
        // Queue the packet for later transmission
        list_add_first(&buf->node, &arp_queue);

        // Perform and arp request
        arp_request(dest_ip, src_ip);

        return;
    }

    // We have a valid mapping on this IPv4 address
    for (u32 i = 0; i < 6; i++)
        *--buf->ptr = dest_mac[5 - i];   
    
    gmac_send_raw(buf);
}

void mac_broadcast(struct netbuf* buf, u16 type)
{
    // Check that the buffers are given in a reight format
    assert(buf->buf + 14 <= buf->ptr);

    // FF:FF:FF:FF:FF:FF
    const u8* src_mac = gmac_get_mac_addr();

    // Fill in the type field
    *--buf->ptr = type & 0xFF;
    *--buf->ptr = (type >> 8) & 0xFF;

    // Fill in the source mac address
    for (u32 i = 0; i < 6; i++)
        *--buf->ptr = src_mac[5 - i];
    
    // Fill in destination mac address
    for (u32 i = 0; i < 6; i++)
        *--buf->ptr = 0xFF;
    
    buf->frame_len += 14;

    // Send the frame
    gmac_send_raw(buf);
}

void mac_copy(const void* src, void* dest)
{
    u8* dest_ptr = dest;
    const u8* src_ptr = src;

    for (u32 i = 0; i < 6; i++) {
        *dest_ptr++ = *src_ptr++;
    }
}
