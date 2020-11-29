/// Copyright (C) strawberryhacker

#ifndef PT_ENTRY_H
#define PT_ENTRY_H

// Section page table entry - level 2 page table
#define STE_PTR_MASK 0b01
#define STE_PTR_BASE(x) ((x) & 0xFFFFFE00)
#define STE_PTR_DOMAIN(domain) (((domain) & 0xF) << 5)

// Section page table entry - section
#define STE_SECTION_MASK 0b10
#define STE_SECTION_BASE(x) ((x) & 0xFFF00000)
#define STE_SECTION_DOMAIN(domain) (((domain) & 0xF) << 5)
#define STE_SECTION_XN (1 << 4)
#define STE_SECTION_nG (1 << 17)

enum ste_mem {
    STE_MEM_STRONGLY_ORDERD = ((0b000 << 12) | (0b00 << 2)),
    STE_MEM_SHARABLE        = ((0b000 << 12) | (0b01 << 2)),
    STE_MEM_WRITE_THROUGH   = ((0b000 << 12) | (0b10 << 2)),
    STE_MEM_WRITE_BACK      = ((0b000 << 12) | (0b11 << 2)),
    STE_MEM_NON_CACHE       = ((0b001 << 12) | (0b00 << 2)),
    STE_MEM_NON_SHAREABLE   = ((0b010 << 12) | (0b00 << 2))
};

enum ste_access {
    STE_ACCESS_NO_ACC       = ((0 << 15) | (0b00 << 10)),
    STE_ACCESS_PRIV_ACC     = ((0 << 15) | (0b01 << 10)),
    STE_ACCESS_NO_USR_WRITE = ((0 << 15) | (0b10 << 10)),
    STE_ACCESS_FULL_ACC     = ((0 << 15) | (0b11 << 10)),
    STE_ACCESS_PRIV_READ    = ((1 << 15) | (0b01 << 10)),
    STE_ACCESS_READ         = ((1 << 15) | (0b10 << 10))
};

// Page table entry
#define PTE_MASK 0b10
#define PTE_BASE(x) ((x) & 0xFFFFF800)

#define PTE_XN (1 << 0)
#define PTE_nG (1 << 11)

enum pte_mem {
    PTE_MEM_STRONGLY_ORDERD = ((0b000 << 6) | (0b00 << 2)),
    PTE_MEM_SHARABLE        = ((0b000 << 6) | (0b01 << 2)),
    PTE_MEM_WRITE_THROUGH   = ((0b000 << 6) | (0b10 << 2)),
    PTE_MEM_WRITE_BACK      = ((0b000 << 6) | (0b11 << 2)),
    PTE_MEM_NON_CACHE       = ((0b001 << 6) | (0b00 << 2)),
    PTE_MEM_NON_SHAREABLE   = ((0b010 << 6) | (0b00 << 2))
};

enum pte_access {
    PTE_ACCESS_NO_ACC       = ((0 << 9) | (0b00 << 4)),
    PTE_ACCESS_PRIV_ACC     = ((0 << 9) | (0b01 << 4)),
    PTE_ACCESS_NO_USR_WRITE = ((0 << 9) | (0b10 << 4)),
    PTE_ACCESS_FULL_ACC     = ((0 << 9) | (0b11 << 4)),
    PTE_ACCESS_PRIV_READ    = ((1 << 9) | (0b01 << 4)),
    PTE_ACCESS_READ         = ((1 << 9) | (0b10 << 4))
};

// LV1 page table section 
#define LV1_PT_SECTION 0b10
#define LV1_PT_SECTION_BASE(x) ((x) & 0xFFF00000)

#define LV1_PT_SECTION_DOMAIN(domain) (((domain) & 0xF) << 5)
#define LV1_PT_SECTION_XN (1 << 4)
#define LV1_PT_SECTION_nG (1 << 17)

#define LV1_PT_SECTION_NO_ACC        (0 << 15) | (0b00 < 10)
#define LV1_PT_SECTION_PRIV_ACC      (0 << 15) | (0b01 < 10)
#define LV1_PT_SECTION_NO_USR_WRITE  (0 << 15) | (0b10 < 10)
#define LV1_PT_SECTION_FULL_ACC      (0 << 15) | (0b11 < 10)
#define LV1_PT_SECTION_PRIV_READ     (1 << 15) | (0b01 < 10)
#define LV1_PT_SECTION_READ          (1 << 15) | (0b10 < 10)

#define LV1_PT_SECTION_STRONGLY_ORDERD   (0b000 << 12) | (0b00 << 2)
#define LV1_PT_SECTION_SHARABLE          (0b000 << 12) | (0b01 << 2)
#define LV1_PT_SECTION_WRITE_THROUGH     (0b000 << 12) | (0b10 << 2)
#define LV1_PT_SECTION_WRITE_BACK        (0b000 << 12) | (0b11 << 2)
#define LV1_PT_SECTION_NON_CACHE         (0b001 << 12) | (0b00 << 2)
#define LV1_PT_SECTION_NON_SHAREABLE     (0b010 << 12) | (0b00 << 2)

// Pointer to the secondary page table 
#define LV1_PT_PTR 0b01
#define LV1_PT_PTR_BASE(x) ((x) & 0xFFFFFE00)
#define LV1_PT_PTR_DOMAIN(domain) (((domain) & 0xF) << 5)

// LV2 small (4k) page table entry 
#define LV2_PT_SECTION 0b10
#define LV2_PT_SECTION_BASE(x) ((x) & 0xFFFFF800)

#define LV2_PT_SECTION_XN (1 << 0)
#define LV2_PT_SECTION_nG (1 << 11)

#define LV2_PT_SECTION_NO_ACC        (0 << 9) | (0b00 << 4)
#define LV2_PT_SECTION_PRIV_ACC      (0 << 9) | (0b01 << 4)
#define LV2_PT_SECTION_NO_USR_WRITE  (0 << 9) | (0b10 << 4)
#define LV2_PT_SECTION_FULL_ACC      (0 << 9) | (0b11 << 4)
#define LV2_PT_SECTION_PRIV_READ     (1 << 9) | (0b01 << 4)
#define LV2_PT_SECTION_READ          (1 << 9) | (0b10 << 4)

#define LV2_PT_SECTION_STRONGLY_ORDERD   (0b000 << 6) | (0b00 << 2)
#define LV2_PT_SECTION_SHARABLE          (0b000 << 6) | (0b01 << 2)
#define LV2_PT_SECTION_WRITE_THROUGH     (0b000 << 6) | (0b10 << 2)
#define LV2_PT_SECTION_WRITE_BACK        (0b000 << 6) | (0b11 << 2)
#define LV2_PT_SECTION_NON_CACHE         (0b001 << 6) | (0b00 << 2)
#define LV2_PT_SECTION_NON_SHAREABLE     (0b010 << 6) | (0b00 << 2)

#endif
