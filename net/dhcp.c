#include <net/dhcp.h>
#include <net/ip.h>
#include <net/netbuf.h>
#include <net/udp.h>
#include <citrus/thread.h>
#include <stddef.h>
#include <citrus/syscall.h>
#include <citrus/mem.h>
#include <citrus/gmac.h>

struct __attribute__((packed)) dhcp_header {
    u8 op;
    u8 htype;
    u8 hlen;
    u8 hops;
    u32 xid;
    u16 secs;
    u16 flags;
    u32 ciaddr;
    u32 yiaddr;
    u32 siaddr;
    u32 giaddr;
    u8 chaddr[16];
    char sname[64];
    u8 file[128];
    u32 magic_cookie;
};

void option_end(u8** data)
{
    **data = 0xFF;
    *data = *data + 1;
}

enum dhcp_type {
    DHCP_DISCOVER = 1,
    DHCP_OFFER,
    DHCP_REQUEST,
    DHCP_DECLINE,
    DHCP_ACK,
    DHCP_NAK,
    DHCP_RELEASE,
    DHCP_INFORM
};

void option_msg_type(u8** data, enum dhcp_type type)
{
    *(*data)++ = 53;     // DHCP type 
    *(*data)++ = 1;      // Lenght
    *(*data)++ = type;   // Type
}

void dhcp_parse(struct netbuf* buf)
{
    char ip[16];

    struct dhcp_header* hdr = (struct dhcp_header *)buf->ptr;

    u32 ip_raw = read_be32(&hdr->yiaddr);

    ipv4_to_str(ip_raw, ip);

    set_ip_addr(ip_raw);

    print("%s\n", ip);
}

// This will send a DHCP discover. The application should be listening on port
// number 68. A DHCP discover will have a broadcast destination IPv4 and a 
// broadcast MAC. The source IPv4 has to be 0.0.0.0
void dhcp_send_discover(u32 tid)
{
    // Allocate a new buffer for use
    struct netbuf* buf = alloc_netbuf();

    struct dhcp_header* hdr = (struct dhcp_header *)buf->ptr;

    hdr->op = 1;
    hdr->htype = 1;
    hdr->hlen = 6;
    hdr->hops = 0;


    store_be32(tid, &hdr->xid);
    store_be16(0, &hdr->secs);
    store_be16(0, &hdr->flags);

    store_be32(0, &hdr->ciaddr);
    store_be32(0, &hdr->yiaddr);
    store_be32(0, &hdr->siaddr);
    store_be32(0, &hdr->giaddr);

    mem_set(&hdr->chaddr, 0x00, 16);
    mem_copy((void *)gmac_get_mac_addr(), &hdr->chaddr, 6);

    mem_set(&hdr->sname, 0x00, 64);
    mem_set(&hdr->file, 0x00, 128);

    // The DHCP is really using BOOTP. The BOOTP has a vendor field. The magic
    // cookie must be the first word in the vendor field (DHCP options)
    store_be32(0x63825363, &hdr->magic_cookie);

    u8* options = (u8 *)(hdr + 1);

    // Add the requeired options
    option_msg_type(&options, DHCP_DISCOVER);
    option_end(&options);

    // Options point after the last byte in the UDP paylaod
    buf->frame_len = options - buf->ptr;

    print("Sending DHCP discover\n");

    udp_send_from(buf, 0xFFFFFFFF, 0x00000000, 68, 67, MAC_BROADCAST);
}

// Main DHCP thread
i32 dhcp_thread(void* arg)
{
    // This is because the first packet is messed up
    syscall_thread_sleep(1000);

    // Listen on the DHCP client port
    udp_listen(68);

    u32 tid = 0xC0DEBABE;
    dhcp_send_discover(tid);

    while (1) {
        
        // Rec from the DHCP client port
        struct netbuf* buf = udp_rec(68);

        if (buf) {
            print("Got a DHCP response\n");
            dhcp_parse(buf);

            free_netbuf(buf);
        }
    }
}

// This will start the main DHCP thread
void dhcp_init(void)
{
    create_kthread(dhcp_thread, 5000, "dhcp", NULL, SCHED_RT);
}
