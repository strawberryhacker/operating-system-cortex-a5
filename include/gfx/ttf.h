// Copyright (C) strawberryhacker

#ifndef TTF_H
#define TTF_H

#include <citrus/types.h>

#define TTF_HEADER_SFNT_VER 0x00010000

#define TTF_NUM_TABLES_OFFSET 4

#define TTF_TABLE_REC_OFFSET 12
#define TTF_TABLE_REC_SIZE 16

#define TTF_TABLE_TAG_OFFSET 0
#define TTF_TABLE_CRC_OFFSET 4
#define TTF_TABLE_OFF_OFFSET 8
#define TTF_TABLE_LEN_OFFSET 12

#define TTF_HEAD_MAGIC            0x5F0F3CF5
#define TTF_HEAD_MAGIC_OFF        12
#define TTF_HEAD_FLAGS_OFF        16
#define TTF_HEAD_UNIT_PM_OFF      18
#define TTF_HEAD_X_MIN_OFF        36
#define TTF_HEAD_Y_MIN_OFF        38
#define TTF_HEAD_X_MAX_OFF        40
#define TTF_HEAD_Y_MAX_OFF        42
#define TTF_HEAD_MAC_STYLE_OFF    44
#define TTF_HEAD_SRP_OFF          46
#define TTF_HEAD_FONT_DIR_OFF     48
#define TTF_HEAD_INDEX_TO_LOC_OFF 50

#define TTF_MAXP_VER_OFF               0
#define TTF_MAXP_NUM_GLYPH_OFF         4
#define TTF_MAXP_MAX_POINTS_OFF        6
#define TTF_MAXP_MAX_COUNTORS_OFF      8
#define TTF_MAXP_MAX_COMP_POINTS_OFF   10
#define TTF_MAXP_MAX_COMP_CONTOURS_OFF 12
#define TTF_MAXP_MAX_ZONES_OFF         14
#define TTF_MAXP_MAX_TWILIGHT_OFF      16
#define TTF_MAXP_MAX_STORAGE_OFF       18
#define TTF_MAXP_MAX_FUNC_DEF_OFF      20
#define TTF_MAXP_MAX_INSTR_DEF_OFF     22
#define TTF_MAXP_MAX_STACK_ELEM_OFF    24
#define TTF_MAXP_MAX_SIZE_INSTR_OFF    26
#define TTF_MAXP_MAX_COMP_ELEM_OFF     28
#define TTF_MAXP_MAX_COMP_DEPTH_OFF    30

#define TTF_HHEA_NUM_METRICS_OFF 34

#define TTF_CMAP_ENC_REC_SIZE        8
#define TTF_CMAP_ENC_REC_PLAT_ID_OFF 0
#define TTF_CMAP_ENC_REC_ENC_ID_OFF  2
#define TTF_CMAP_ENC_REC_OFFSET_OFF  4


// Required table tags
#define TTF_TAG_CMAP 0x636d6170
#define TTF_TAG_HEAD 0x68656164
#define TTF_TAG_HHEA 0x68686561
#define TTF_TAG_HMTX 0x686d7478
#define TTF_TAG_MAXP 0x6d617870
#define TTF_TAG_NAME 0x6e616d65
#define TTF_TAG_OS2  0x4f532f32
#define TTF_TAG_POST 0x706f7374

// TrueType table tags
#define TTF_TAG_CVT  0x63767420
#define TTF_TAG_FPGM 0x6670676d
#define TTF_TAG_GLYF 0x676c7966
#define TTF_TAG_LOCA 0x6c6f6361
#define TTF_TAG_PREP 0x70726570
#define TTF_TAG_GASP 0x67617370

struct font {
    
};

i32 ttf_thread(void* arg);

#endif