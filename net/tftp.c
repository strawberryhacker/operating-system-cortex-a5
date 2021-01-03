#include <net/tftp.h>
#include <net/netbuf.h>
#include <net/ip.h>
#include <net/udp.h>
#include <citrus/mem.h>
#include <citrus/panic.h>
#include <citrus/print.h>
#include <citrus/error.h>
#include <citrus/kmalloc.h>

#define TFTP_READ    1
#define TFTP_WRITE   2
#define TFTP_DATA    3
#define TFTP_ACK     4
#define TFTP_ERR     5
#define TFTP_OPT_ACK 6

// The IP address can be gotten from the DHCP response

i32 tftp_create(struct tftp* t, const char* ip_addr)
{
    i32 err = str_to_ipv4(ip_addr, &t->server_ip);
    if (err)
        return err;

    udp_get_free_port(&t->client_port);
    udp_listen(t->client_port);

    return 0;
}

// This will add the option at the given location and move the pointer so it's
// pointng to the next byte
static void tftp_add_field(u8** data, const char* opt)
{
    while (*opt)
        *(*data)++ = *opt++;

    *(*data)++ = 0x00;
}

// This takes in a netbuf pointing to the beginning of the UDP payload
static i32 get_filesize(struct netbuf* buf, u32* filesize)
{
    u8* ptr = buf->ptr;
    if (read_be16(ptr) != TFTP_OPT_ACK)
        return -ENET;

    ptr += 2;
    i32 len = buf->frame_len - 2;

    // [ opt ] [ ascii: tsize ] [ 0 ] [ ascii: file_size ] [ 0 ]

    const char* tsize = "tsize";

    for (u32 i = 0; i < 5 && len; i++, len--) {
        if (*ptr++ != tsize[i])
            return -ENET;
    }
    if (*ptr++ != 0x00)
        return -ENET;
    
    len--;

    // Extraxt the length of the file. This is NULL terminated and in ASCII
    u32 tmp = 0;
    while (len-- && *ptr)
        tmp = (tmp * 10) + (*ptr++ - '0');

    if (len < 0)
        return -ENET;

    *filesize = tmp;

    return 0; 
}

void ack_opt_ack(struct tftp* t)
{
    struct netbuf* buf = alloc_netbuf();

    u8* ptr = buf->ptr;

    // Read operation
    store_be32(0x00040000, ptr);
    buf->frame_len = 4;

    udp_send_from(buf, t->server_ip, get_src_ip(), t->client_port, 
        t->server_port, 0);
}

void send_ack(struct tftp* t, u32 block)
{
    struct netbuf* buf = alloc_netbuf();

    u8* ptr = buf->ptr;

    // Read operation
    store_be16(0x0004, ptr);
    store_be16(block, ptr + 2);
    buf->frame_len = 4;

    udp_send_from(buf, t->server_ip, get_src_ip(), t->client_port,
        t->server_port, 0);
}

i32 tftp_download(struct tftp* t, const char* filename, u8** file, u32* size)
{
    assert(t);

    struct netbuf* buf = alloc_netbuf();

    u8* ptr = buf->ptr;

    // Read operation
    store_be16(TFTP_READ, ptr);
    ptr += 2;

    tftp_add_field(&ptr, filename);
    tftp_add_field(&ptr, "octet");
    tftp_add_field(&ptr, "tsize");   // Option for getting the file size
    tftp_add_field(&ptr, "0");       // Zero in the request

    // Compte the size of the packet 
    buf->frame_len = ptr - buf->ptr;

    udp_send_from(buf, t->server_ip, get_src_ip(), t->client_port, 69, 0);

    print("Send a TFTP req\n");

    u32 filesize = 0;

    // Get the file size
    while (1) {

        struct netbuf* buf = udp_rec(t->client_port);

        if (buf) {
            i32 err = get_filesize(buf, &filesize);

            // If noe error send the ACK
            if (err) {
                free_netbuf(buf);
                return err;
            } 

            t->server_port = read_be16(buf->ptr - 8);
            ack_opt_ack(t);

            // Free the netbuf
            free_netbuf(buf);

            break;
        }
    }

    // Allocate a new buffer
    u8* buffer = kmalloc(filesize);

    *file = buffer;
    *size = filesize;

    u32 block_number = 1;


    print("Listening on port %d\n", t->client_port);

    // Read the content of the file info the pointer
    while (1) {
        struct netbuf* buf = udp_rec(t->client_port);

        if (buf) {

            print("New buffer\n");
            // Read the raw data
            if (read_be16(buf->ptr) != TFTP_DATA)
                panic("Not data");

            if (read_be16(buf->ptr + 2) != block_number)
                panic("Block number wrong\n");

            u32 len = buf->frame_len - 4;

            mem_copy(buf->ptr + 4, buffer, len);
            buffer += len;

            // Send the ACK
            send_ack(t, block_number);

            block_number++;

            if (len != 512)
                return 0;
        }
    }
}
