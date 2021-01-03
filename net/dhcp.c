#include <net/dhcp.h>
#include <net/ip.h>
#include <net/netbuf.h>
#include <net/udp.h>
#include <citrus/thread.h>
#include <stddef.h>
#include <citrus/syscall.h>
#include <citrus/mem.h>
#include <citrus/gmac.h>
#include <citrus/error.h>
#include <citrus/kmalloc.h>
#include <citrus/panic.h>

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
    DHCP_INFORM,
    DHCP_NONE
};

void option_msg_type(u8** data, enum dhcp_type type)
{
    *(*data)++ = 53;     // DHCP type 
    *(*data)++ = 1;      // Lenght
    *(*data)++ = type;   // Type
}

void option_set_server_indentifier(u8** data, u32 ip)
{
    *(*data)++ = 54;     // Option type
    *(*data)++ = 4;      // Lenght
    
    store_be32(ip, *data);
    (*data) += 4;
}

void option_set_req_ip(u8** data, u32 ip)
{
    *(*data)++ = 50;     // Option type
    *(*data)++ = 4;      // Lenght
    
    store_be32(ip, *data);
    (*data) += 4;
}

#define BOOTREQ   1
#define BOOTREPLY 2

#define DHCP_MAGIC_COOKIE 0x63825363

#define DHCP_OPT_SUBNET_MASK 1
#define DHCP_OPT_ROUTER      3
#define DHCP_OPT_DNS         6
#define DHCP_OPT_LEASE_TIME  51

// This will get a double pointer ot the start of the option field. It will try
// to find a spesific option.
//
// If the option is found it returns 0 and modifies the option pointer, if not 
// found it returns -E and leave the pointer unchanged
i32 get_option_ptr(const u8** opt_ptr, u32 opt_len, u8 opt_value)
{
    if (opt_len < 4)
        return -ENOENT;

    const u8* opt = *opt_ptr;

    // Check the magic cookie
    if (read_be32(opt) != DHCP_MAGIC_COOKIE)
        return -ENOENT;
    
    opt_len -= 4;
    opt += 4;

    // The option fields are ternimated with a 0xFF
    //
    //  [ opt number ] [ size ] [ data1 ] [ data2 ] [ * ] ... [ 0xFF ]
    while (1) {
        // This will make sure we can read the two first values
        if (opt_len < 2)
            return -ENOENT;

        // Store the pointer and the value of the option number
        const u8* opt_num_ptr = opt;
        u8 opt_num = *opt++;

        // Check for the last option
        if (opt_num == 0xFF)
            return -ENOENT;

        u8 len = *opt++;
        opt_len -= 2;

        // Jump past the data
        if (opt_len < len)
            return -ENOENT;
        
        // Check if the type matches
        if (opt_num == opt_value) {
            *opt_ptr = opt_num_ptr;
            return 0;
        }

        opt_len -= len;
        opt += len;
    }
}

// Returns the DHCP type. If this option is not present it returns DHCP_NONE
enum dhcp_type get_dhcp_type(const u8* opt_ptr, u32 opt_len)
{
    i32 err = get_option_ptr(&opt_ptr, opt_len, 53);
    if (err)
        return DHCP_NONE;

    // Jump past the size and the opt number
    opt_ptr += 2;

    return (enum dhcp_type)*opt_ptr;
}

// This takes in the start of the DHCP option field and returns the subnet mask
i32 get_subnet_mask(const u8* opt_ptr, u32 opt_len, u32* subnet_mask)
{
    i32 err = get_option_ptr(&opt_ptr, opt_len, DHCP_OPT_SUBNET_MASK);
    if (err)
        return err;

    opt_ptr += 2;

    *subnet_mask = read_be32(opt_ptr);
    return 0;
}

// This will try to get the gatewy from the option field. This will perform an 
// allocation and return the pointer. If returned -ENOENT no allocation will be
// done
i32 get_gateway(const u8* opt_ptr, u32 opt_len, u32** gateway, u32* gateway_cnt)
{
    i32 err = get_option_ptr(&opt_ptr, opt_len, DHCP_OPT_ROUTER);
    if (err)
        return err;

    opt_ptr++;

    u32 cnt = *opt_ptr++ / 4;
    if (cnt == 0)
        return -ENOENT;

    u32* g = kmalloc(sizeof(u32) * cnt);

    // Read all the gateways
    for (u32 i = 0; i < cnt; i++) {
        g[i] = read_be32(opt_ptr);
        opt_ptr += 4;
    }

    *gateway = g;
    *gateway_cnt = cnt;
    return 0;
}

// This will take in a netbuf pointing to the DHCP header (UDP payload)
void handle_dhcp_offer(struct netbuf* buf, struct ip_struct* ip)
{
    print("Handling DHCP offset request\n");

    struct dhcp_header* hdr = (struct dhcp_header *)buf->ptr;

    // Parse the DHCP offer info
    ip->our_ip = read_be32(&hdr->yiaddr);

    // Check that we have any options
    if (buf->frame_len <= sizeof(struct dhcp_header))
        return;
    
    u32 opt_len = buf->frame_len - sizeof(struct dhcp_header);
    const u8* opt_ptr = (const u8 *)(hdr + 1);

    // Gettting some options
    i32 err = get_subnet_mask(opt_ptr, opt_len, &ip->subnet_mask);
    if (err)
        print("Cant get subnet mask\n");
    
    err = get_gateway(opt_ptr, opt_len, &ip->server_ip, &ip->server_cnt);

    ip_print(ip);
}

u8 is_dhcp_reply_valid(struct netbuf* buf, u32 tid)
{
    // We dont supprt BOOTP requests with no DHCP tag
    if (buf->frame_len <= sizeof(struct dhcp_header))
        return 0;

    // Get a pointer to the header
    struct dhcp_header* hdr = (struct dhcp_header *)buf->ptr;

    // We don't act as a DHCP server
    if (hdr->op != BOOTREPLY)
        return 0;

    // Ethernet medium
    if (hdr->htype != 1)
        return 0;
    
    // MAC address should be 6 bytes
    if (hdr->hlen != 6)
        return 0;

    // Check for matching transaction ID number
    if (read_be32(&hdr->xid) != tid)
        return 0;
    
    return 1;
}

// Parses an incoming DHCP request. This will not free any netbuf
i32 dhcp_parse_offer(struct netbuf* buf, u32 tid, struct ip_struct* ip_struct)
{
    if (!is_dhcp_reply_valid(buf, tid))
        return -ENET;

    struct dhcp_header* hdr = (struct dhcp_header *)buf->ptr;
    const u8* opt = (const u8 *)(hdr + 1);

    // Check the DHCP type
    enum dhcp_type type = 
        get_dhcp_type(opt, buf->frame_len - sizeof(struct dhcp_header));

    if (type != DHCP_OFFER) 
        return -ENET;

    handle_dhcp_offer(buf, ip_struct);
    return 0;
}

i32 dhcp_parse_ack(struct netbuf* buf, u32 tid)
{
    if (!is_dhcp_reply_valid(buf, tid))
        return -ENET;

    struct dhcp_header* hdr = (struct dhcp_header *)buf->ptr;
    const u8* opt = (const u8 *)(hdr + 1);

    // Check the DHCP type
    enum dhcp_type type = 
        get_dhcp_type(opt, buf->frame_len - sizeof(struct dhcp_header));

    if (type != DHCP_ACK) 
        return -ENET;

    print("Got and ACK\n");
    return 0;
}

struct netbuf* dhcp_request_template(u32 tid)
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
    store_be32(0x63825363, hdr + 1);

    return buf;
}

// This will send a DHCP discover. The application should be listening on port
// number 68. A DHCP discover will have a broadcast destination IPv4 and a 
// broadcast MAC. The source IPv4 has to be 0.0.0.0. 
void dhcp_send_discover(u32 tid)
{
    struct netbuf* buf = dhcp_request_template(tid);
    struct dhcp_header* hdr = (struct dhcp_header *)buf->ptr;

    // This will start after the magic cookie
    u8* options = (u8 *)(hdr + 1) + 4;

    // Add the requeired options
    option_msg_type(&options, DHCP_DISCOVER);
    option_end(&options);

    // Options point after the last byte in the UDP paylaod
    buf->frame_len = options - buf->ptr;

    print("Sending DHCP discover\n");

    udp_send_from(buf, 0xFFFFFFFF, 0x00000000, 68, 67, MAC_BROADCAST);
}

void dhcp_send_request(u32 tid, u32 server_ip, u32 granted_ip)
{
    struct netbuf* buf = dhcp_request_template(tid);
    struct dhcp_header* hdr = (struct dhcp_header *)buf->ptr;

    // This will start after the magic cookie
    u8* options = (u8 *)(hdr + 1) + 4;

    // Add the requeired options
    option_msg_type(&options, DHCP_REQUEST);
    option_set_server_indentifier(&options, server_ip);
    option_set_req_ip(&options, granted_ip);
    option_end(&options);

    // Options point after the last byte in the UDP paylaod
    buf->frame_len = options - buf->ptr;

    print("Sending DHCP request\n");

    udp_send_from(buf, 0xFFFFFFFF, 0x00000000, 68, 67, MAC_BROADCAST);
}

// Main DHCP thread
i32 dhcp_thread(void* arg)
{
    // This is because the first packet is messed up
    syscall_thread_sleep(1000);

    // Listen on the DHCP client port
    udp_listen(68);

    struct ip_struct ip_struct;

    while (1) {
        u32 tid = 0xC0DEBABE;
        dhcp_send_discover(tid);

        while (1) {
            // Rec from the DHCP client port
            struct netbuf* buf = udp_rec(68);

            if (buf) {
                print("Got a DHCP response\n");
                i32 err = dhcp_parse_offer(buf, tid, &ip_struct);
                free_netbuf(buf);
                if (err == 0) {
                    break;
                }
            }

            // Do this for 2 sec. then send another discover
        }

        assert(ip_struct.server_cnt);
        dhcp_send_request(tid, ip_struct.server_ip[0], ip_struct.our_ip);

        while (1) {
            // Rec from the DHCP client port
            struct netbuf* buf = udp_rec(68);

            if (buf) {
                print("Got a DHCP response\n");
                i32 err = dhcp_parse_ack(buf, tid);
                free_netbuf(buf);
                if (err == 0) {
                    break;
                }
            }
        }
        
        set_ip_struct(&ip_struct);

        while (1)
            syscall_thread_sleep(1000);
    }
}

// This will start the main DHCP thread
void dhcp_init(void)
{
    create_kthread(dhcp_thread, 5000, "dhcp", NULL, SCHED_RT);
}
