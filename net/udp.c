
#include <net/udp.h>
#include <citrus/mem.h>

// Please do bound checking on the lenghts

void udp_receive(struct netbuf* buf)
{
    print("UDP packet\n");
    for (u32 i = 0; i < buf->frame_len; i++) {
        print("%02x ", buf->ptr[i]);
    }
    print("\n\n");

    u16 length = read_be16(buf->ptr + 4) - 8;

    u16 src_port = read_be16(buf->ptr + 0);
    u16 dest_port = read_be16(buf->ptr + 2);

    buf->ptr += 8;

    print("receive UDP from port %d to port %d\n", src_port, dest_port);

    for (u32 i = 0; i < length; i++) {
        print("%c", buf->ptr[i]);
    }
    print("\n");
}
