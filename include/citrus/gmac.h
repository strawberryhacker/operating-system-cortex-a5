#ifndef GMAC_H
#define GMAC_H

#include <citrus/types.h>

void gmac_init(void);

i32 phy_discover(u16 id, u16* addr);
u16 phy_read(u8 addr, u8 reg);
void phy_write(u8 addr, u8 reg, u16 data);

#endif