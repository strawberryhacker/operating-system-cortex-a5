#include <net/dhcp.h>
#include <net/ip.h>
#include <net/netbuf.h>
#include <net/udp.h>
#include <citrus/thread.h>
#include <stddef.h>
#include <citrus/syscall.h>
#include <citrus/mem.h>
#include <citrus/gmac.h>

struct dhcp_header {
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

void option_msg_type(u8** data)
{
    *(*data)++ = 53;
    *(*data)++ = 1;
    *(*data)++ = 1;
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

i32 dhcp_thread(void* arg)
{
    syscall_thread_sleep(1000);

    struct netbuf* buf = alloc_netbuf();

    struct dhcp_header* hdr = (struct dhcp_header *)buf->ptr;

    hdr->op = 1;
    hdr->htype = 1;
    hdr->hlen = 6;
    hdr->hops = 0;

    store_be32(0xC0DEBABE, &hdr->xid);
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

    store_be32(0x63825363, &hdr->magic_cookie);

    // Options
    u8* options = (u8 *)(hdr + 1);
    option_msg_type(&options);
    option_end(&options);

    buf->frame_len = options - buf->ptr;

    print("Sending DHCP discover\n");
    udp_send_from(buf, 0xFFFFFFFF, 0x00000000, 68, 67, MAC_BROADCAST);

    udp_listen(68);

    while (1) {
        
        struct netbuf* buf = udp_rec(68);

        if (buf) {
            print("Got a DHCP response\n");
            dhcp_parse(buf);
        }
    }
}

void dhcp_init(void)
{
    create_kthread(dhcp_thread, 5000, "dhcp", NULL, SCHED_RT);   
}
