#include <net/netbuf.h>
#include <citrus/kmalloc.h>
#include <citrus/print.h>
#include <citrus/panic.h>
#include <citrus/atomic.h>

#define MAC_HEADER_SIZE 100

struct list_node netbuf_pool;

void netbuf_init(void)
{
    list_init(&netbuf_pool);

    assert(netbuf_pool.next == &netbuf_pool);
    
    for (u32 i = 0; i < 1024; i++) {
        struct netbuf* buf = kmalloc(sizeof(struct netbuf));

        if (buf == NULL)
            panic("Mem error");

        // Add the packets to the listned list
        list_add_first(&buf->node, &netbuf_pool);
    }
}

// Allocated a netbuffer
struct netbuf* alloc_netbuf(void)
{
    struct netbuf* buf = NULL;

    // Check if we have any more netbuffers
    if (netbuf_pool.next != &netbuf_pool) {

        struct list_node* node = netbuf_pool.next;
        buf = list_get_entry(node, struct netbuf, node);
        buf->ptr = buf->buf + MAX_HEADER_SIZE;

        list_delete_first(&netbuf_pool);
    }
    return buf;
}

// Freeing a netbuf
void free_netbuf(struct netbuf* buf)
{
    list_add_first(&buf->node, &netbuf_pool);
}

// TMP
struct list_node* get_netbuf_list(void)
{
    return &netbuf_pool;
}
