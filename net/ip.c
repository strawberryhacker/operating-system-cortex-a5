#include <net/ip.h>
#include <citrus/error.h>

// Converts an IPv4 address from a binary representation to a string 
// representation "xxx.xxx.xxx.xxx". 
// The input buffer has to be at least 16 bytes long
void ipaddr_to_str(ipaddr_t addr, char* buf)
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
i32 str_to_ipaddr(const char* buf, ipaddr_t* addr)
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
