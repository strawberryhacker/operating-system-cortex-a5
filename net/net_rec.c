#include <net/net_rec.h>
#include <citrus/gmac.h>
#include <net/netbuf.h>
#include <net/arp.h>
#include <citrus/thread.h>
#include <stddef.h>
#include <citrus/atomic.h>

static u8 buffer[1538];

struct netbuf netbuf;

static i32 net_rec_thread(void* arg)
{
    while (1) {
        i32 err = gmac_rec_raw(buffer, &netbuf.frame_len);

        if (err)
            continue;

        // We have to check the Type field of the MAC frame to determine 
        // whether to pass it to the ARP module or to the IP stack
        struct mac_header* mac_header = (struct mac_header *)buffer;

        // Set up the netbuffer. We make sure the header is pointer to the 
        // same place as the buffer. This is because the MAC layer is the 
        // first one
        netbuf.header = netbuf.buf;

        if (mac_header->type == 0x0608) {
            // Received fragment is an ARP packet
            arp_handle(&netbuf);
        } else if (mac_header->type == 0x0008) {
            // Recevied fragment is an IPv4 packet
        }
    }
    return 0;
}

void net_rec_init(void)
{

    // Intialize the net buffer
    netbuf.buf = buffer;
    netbuf.buf_len = 1538;

    create_kthread(net_rec_thread, 500, "net_rec", NULL, SCHED_RT);
}
