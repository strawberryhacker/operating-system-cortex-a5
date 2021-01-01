#ifndef GMAC_H
#define GMAC_H

#include <citrus/types.h>
#include <net/netbuf.h>

enum phy_speed {
    PHY_100BASE,
    PHY_10BASE
};

enum phy_duplex {
    PHY_FULL_DUPLEX,
    PHY_HALF_DUPLEX
};

struct gmac_desc {
    u32 addr;
    u32 status;
};

// Receive descriptor for each receive buffer
struct gmac_rx_desc {
    union {
        u32 addr_word;
        struct {
            // This bit is set to 1 after a receive transfer indicating that the
            // buffer has been used and now belongs to the software. The 
            // software then has to clear this bit before using it again.
            u32 owner_software     : 1;
            u32 wrap               : 1;
            u32 addr               : 30;
        };
    };
    union {
        u32 status_word;
        struct {
            u32 length             : 13;
            u32 fcs_status         : 1;
            u32 sof                : 1;
            u32 eof                : 1;
            u32 cfi_bit            : 1;
            u32 vlan_pri           : 3;
            u32 priority_tag       : 1;
            u32 vlan_tag           : 1;
            u32 type_id_reg        : 2;
            u32 type_id_match_snap : 1;
            u32 addr_match_reg     : 2;
            u32 addr_match         : 1;
            u32 reserved0          : 1;
            u32 unicast_hash       : 1;
            u32 multicast_hash     : 1;
            u32 broadcast_detected : 1;            
        };
    };
};

// Transmit descriptor for each transmit buffer
struct gmac_tx_desc {

    // This is the byte address of the buffer
    u32 addr;
    union {
        u32 status_word;
        struct  {
            u32 length         : 14;
            u32 reserved0      : 1;
            u32 last_buffer    : 1;
            u32 no_crc         : 1;
            u32 reserved1      : 3;
            u32 crc_gen_err    : 3;
            u32 reserved2      : 3;
            u32 late_collision : 1;
            u32 ahb_err        : 1;
            u32 reserved3      : 1;
            u32 retry_exceeded : 1;
            u32 wrap           : 1;

            // This bit is set to 1 after a receive transfer indicating that the
            // buffer has been used and now belongs to the software. The 
            // software then has to clear this bit before using it again.
            u32 owner_software : 1;
        };
    };
};

#define GMAC_RX_ADDR_MASK             0x3FFFFFFF
#define GMAC_RX_WRAP_MASK             (1 << 1)
#define GMAC_RX_OWNER_MASK            (1 << 0)
#define GMAC_RX_BROADCAST_MATCH_MASK  (1 << 31)
#define GMAC_RX_MULTICAST_MATCH_MASK  (1 << 30)
#define GMAC_RX_UNICAST_MATCH_MASK    (1 << 29)
#define GMAC_RX_ADDR_MATCH_MASK        27
#define GMAC_RX_ADDR_REG_MASK         0b11
#define GMAC_RX_ADDR_REG_POS          (1 << 25)
#define GMAC_RX_ID_MATCH_MASK         (1 << 24)
#define GMAC_RX_SNAP_MASK             (1 << 24)
#define GMAC_RX_ID_MASK               0b11
#define GMAC_RX_ID_POS                22
#define GMAC_RX_CRC_STATUS_MASK       0b11
#define GMAC_RX_CRC_STATUS_POS        22
#define GMAC_RX_VLAN_TAG_MASK         (1 << 21)
#define GMAC_RX_PRI_TAG_MASK          (1 << 20)
#define GMAC_RX_VLAN_PRI_MASK         0b111
#define GMAC_RX_VLAN_PRI_POS          17
#define GMAC_RX_CFI_MASK              (1 << 16)
#define GMAC_RX_EOF_MASK              (1 << 15)
#define GMAC_RX_SOF_MASK              (1 << 14)
#define GMAC_RX_JUMBO_FRAME_LEN_MASK  0b11111111111111
#define GMAC_RX_FRAME_LEN_MASK        0b1111111111111
#define GMAC_RX_FCS_STATUS_MASK       (1 << 13)
#define GMAC_TX_USED_MASK             (1 << 31)
#define GMAC_TX_WRAP_MASK             (1 << 30)
#define GMAC_TX_ERROR_MASK            (1 << 29)
#define GMAC_TX_AHB_ERROR_MASK        (1 << 27)
#define GMAC_TX_LATE_COLLISION_MASK   (1 << 26)
#define GMAC_TX_CRC_ERROR_STATUS_POS  20
#define GMAC_TX_CRC_ERROR_STATUS_MASK 0b111
#define GMAC_TX_NO_CRC_MASK           (1 << 16)
#define GMAC_TX_LAST_MASK             (1 << 15)
#define GMAC_TX_BUFFER_LEN_MASK       0b11111111111111

void gmac_init(void);

i32 phy_discover(u16 id, u16* addr);
u16 phy_read(u8 addr, u8 reg);
void phy_write(u8 addr, u8 reg, u16 data);
void phy_enable_100baseT(u8 addr);
void phy_get_cfg(u8 addr, enum phy_speed* speed, enum phy_duplex* dup);

void gmac_send_raw(struct netbuf* buf);
i32 gmac_rec_raw(struct netbuf** buf);

const u8* gmac_get_mac_addr(void);

#endif
