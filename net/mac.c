#include <net/mac.h>
#include <net/netbuf.h>
#include <citrus/gmac.h>
#include <net/arp.h>
#include <citrus/panic.h>
#include <citrus/mem.h>

void mac_receive(struct netbuf* buf)
{
    // We don't need to check the source or destination mac address since this 
    // is filtered by the GMAC hardware

    // Skip the source and destination MAC
    buf->ptr += 12;

    // Get the MAC type field
    u16 type = read_be16(buf->ptr);
    buf->ptr += 2;

    // Shrink with the MAC header length
    buf->frame_len -= 14;

    if (type == 0x0806) {
        arp_receive(buf);

    } else {
        ip_receive(buf);    
    }
}

// Fill in the MAC frame and get the dest MAC address form the ARP module
void mac_send(struct netbuf* buf, ipaddr_t dest_ip, ipaddr_t src_ip, u16 type)
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

    // Get the destination MAC address from the ARP table
    u8 dest_mac[6];
    i32 err = arp_search(dest_ip, dest_mac);

    if (err) {
        buf->ptr -= 6;
        
        // Queue the packet for later transmission

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
