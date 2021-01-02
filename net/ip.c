#include <net/ip.h>
#include <citrus/error.h>
#include <citrus/mem.h>
#include <net/udp.h>
#include <net/mac.h>

// Converts an IPv4 address from a binary representation to a string 
// representation "xxx.xxx.xxx.xxx". 
// The input buffer has to be at least 16 bytes long
void ipv4_to_str(ipaddr_t addr, char* buf)
{
    u8 shift = 24;
    for (u32 i = 0; i < 4; i++) {
        u8 frag = (addr >> shift) & 0xFF;
        shift -= 8;

        u32 div = 100;
        for (u32 j = 0; j < 3; j++) {
            
            u8 tmp = frag / div;
            if (tmp || j == 2)
                *buf++ = tmp + '0';

            frag = frag % div;
            div /= 10;
        }

        if (i != 3)
            *buf++ = '.';
    }
    *buf = '\0';
}

// Converts a string to an IPv4 address. The string must be on format 
// "xxx.xxx.xxx.xxx"
i32 str_to_ipv4(const char* buf, ipaddr_t* addr)
{
    for (u32 i = 0; i < 4; i++) {
        u32 octet = 0;

        do {
            
            // Check if the number if in range
            if (*buf < '0' || *buf > '9')
                return -EBADIP;
            
            octet = (octet * 10) + (*buf++ - '0');

            if (octet > 255)
                return -EBADIP;

        } while (*buf != ((i == 3) ? 0 : '.'));

        // Jump past the dot
        buf++;

        *addr = (*addr << 8) | octet;
    }

    return 0;
}

void ip_receive(struct netbuf* buf)
{
    // Check the length of the payload
    u16 size = read_be16(buf->ptr + 2);

    print("Got an IPv4 packet with size %d\n", size);

    u8 protocol = buf->ptr[9];

    // Get the length of the header
    u32 header_len = (buf->ptr[0] & 0xF) * 4;

    // Move the pointer to the payload
    buf->ptr += header_len;
    buf->frame_len = size - header_len;

    // Check the protocol
    if (protocol == 0x11) {

        // UPD protocol
        udp_handle(buf);
    }
}

static u32 our_ip = 0;

void set_ip_addr(u32 ip)
{
    our_ip = ip;
}

u32 get_src_ip(void)
{
    u32 ip;
    str_to_ipv4("192.168.0.14", &ip);
    return our_ip;
}

void ip_send(struct netbuf* buf, u32 src_ip, u32 dest_ip, u8 flags)
{
    buf->frame_len += 20;

    buf->ptr -= 4;
    store_be32(dest_ip, buf->ptr);

    buf->ptr -= 4;
    store_be32(src_ip, buf->ptr);

    buf->ptr -= 3;

    // UDP
    *buf->ptr-- = 0x11;

    *buf->ptr = 10;

    buf->ptr -= 2;

    store_be16(0x4000, buf->ptr);

    buf->ptr -= 2;
    store_be16(0, buf->ptr);

    buf->ptr -= 2;
    store_be16(buf->frame_len, buf->ptr);

    *--buf->ptr = 0;
    *--buf->ptr = (4 << 4) | 5;

    u8 broad = (flags & MAC_BROADCAST) ? 1 : 0;
    mac_send(buf, dest_ip, src_ip, 0x0800, broad);   
}
