#ifndef GMAC_H
#define GMAC_H

#include <citrus/types.h>

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

#endif