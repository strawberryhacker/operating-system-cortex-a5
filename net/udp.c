
#include <net/udp.h>
#include <citrus/mem.h>
#include <citrus/kmalloc.h>
#include <stddef.h>
#include <net/ip.h>
#include <net/mac.h>
#include <citrus/error.h>
#include <citrus/panic.h>
#include <citrus/mem.h>
#include <citrus/atomic.h>

// Global UDP module
static struct udp udp;

void udp_init(void)
{
    list_init(&udp.ports);
}

// This will add a new packet last in the port buffer
void add_netbuf_to_port(struct netbuf* buf, u16 port)
{
    struct list_node* node;
    list_iterate(node, &udp.ports) {

        struct udp_port* p = list_get_entry(node, struct udp_port, node);

        if (p->port == port) {
            list_add_last(&buf->node, &p->packets);
        }
    }
}

// This will pop the most recent packet from the port queue
struct netbuf* get_netbuf_from_port(u16 port)
{
    struct list_node* node;
    list_iterate(node, &udp.ports) {

        struct udp_port* p = list_get_entry(node, struct udp_port, node);

        if (p->port == port) {

            if (p->packets.next == &p->packets)
                return NULL;

            struct list_node* first_node = p->packets.next;
            list_delete_first(&p->packets);
            return list_get_entry(first_node, struct netbuf, node);
        }
    }
    return NULL;
}

// This is called by the application. This will check the port buffer first, 
// if non-enpty buffer it will reurn the first element. If the buffer is 
// empty is will do a mac_receive call. This will propagate the packet up the 
// stack and maybe end up in the port packet buffer. Then it will try to read
// again. 
struct netbuf* udp_rec(u16 port)
{
    // Check packet buffer
    struct netbuf* buf = get_netbuf_from_port(port);
    if (buf) {
        return buf;
    }
    
    // Do a raw read
    mac_receive();

    // Check packet buffer again
    buf = get_netbuf_from_port(port);
    if (buf) {
        return buf;
    }

    return NULL;
}

// Final step if this is a UDP packet
void udp_handle(struct netbuf* buf)
{
    u16 length = read_be16(buf->ptr + 4) - 8;

    // Get the port number for the application
    u16 src_port = read_be16(buf->ptr + 0);
    u16 dest_port = read_be16(buf->ptr + 2);

    // Strip the UDP header
    buf->ptr += 8;
    buf->frame_len -= 8;

    // Just add the buffer to the port qeueue. This is made if one application
    // is listening for that port. 
    //
    // If the application calls read. This will first do a MAC call whlich 
    // will end up in calling this function. Then reading from the packet 
    // buffer, eventually catch the newly added packet
    add_netbuf_to_port(buf, dest_port);
}

static u16 udp_port = 1000;

void udp_get_free_port(u16* port)
{
    u32 atomic = __atomic_enter();
    *port = udp_port;
    udp_port++;
    __atomic_leave(atomic);
}

// Make a new port link
void udp_listen(u16 port)
{
    struct list_node* node;
    list_iterate(node, &udp.ports) {
        struct udp_port* p = list_get_entry(node, struct udp_port, node);

        if (p->port == port) {
            panic("UDP port error\n");
        } 
    }

    struct udp_port* udp_port = kmalloc(sizeof(struct udp_port));

    // Initialize the port packet list
    list_init(&udp_port->packets);

    // Add the port to the global UDP structure
    list_add_first(&udp_port->node, &udp.ports);

    udp_port->port = port;
}

// This takes in a packet buffer. The frame_length field in the netbuf should
// be set to the total packet size
void udp_send(struct netbuf* buf, u32 ip, u16 port, u8 flags)
{
    // Skip the UDP CRC. This will be added by the hardware
    buf->ptr -= 4;

    // Length of the UDP packet including UDP header
    store_be16(buf->frame_len + 8, buf->ptr);
    buf->ptr -= 2;

    // Destination port
    store_be16(port, buf->ptr);
    buf->ptr -= 2;

    // Source port
    store_be16(port, buf->ptr);

    buf->frame_len += 8;

    u32 src_ip = get_src_ip();
    ip_send(buf, src_ip, ip, flags);
}

void udp_send_from(struct netbuf* buf, u32 dest_ip, u32 src_ip, u16 src_port, 
    u16 dest_port, u8 flags)
{
    buf->ptr -= 4;

    // Lenght of the UDP packet including UDP header
    store_be16(buf->frame_len + 8, buf->ptr);
    buf->ptr -= 2;

    // Destination port
    store_be16(dest_port, buf->ptr);
    buf->ptr -= 2;

    // Source port
    store_be16(src_port, buf->ptr);

    buf->frame_len += 8;

    ip_send(buf, src_ip, dest_ip, flags);
}
