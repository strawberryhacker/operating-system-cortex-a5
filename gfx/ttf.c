

#include <citrus/fs.h>
#include <citrus/thread.h>
#include <citrus/syscall.h>
#include <citrus/panic.h>
#include <citrus/kmalloc.h>
#include <citrus/fat.h>
#include <citrus/mem.h>
#include <citrus/align.h>
#include <citrus/error.h>

#include <citrus/lcd.h>

#include <gfx/ttf.h>

struct ttf_table_info {
    u32 off;
    u32 len;
};

struct ttf_head {
    u16 flags;
    u16 units_per_em;

    i16 x_min;
    i16 y_min;
    i16 x_max;
    i16 y_max;

    u16 mac_style;

    // Smalles readable size in pixels
    u16 srp;
    i16 dir_hint;

    // 0 for short offset and 1 for long offset. This is used when reading the
    // info about the glyphs
    i16 index_to_loc_fmt;
};

// Info about the maximum profile. This is mainly used to determine the memory
// to be allocated
struct ttf_maxp {
    u16 num_glyphs;
    u16 max_points;
    u16 max_contors;
    u16 max_comp_points;
    u16 max_comp_contours;
    u16 max_zones;
    u16 max_twilight_points;
    u16 max_storage;
    u16 max_func_defs;
    u16 max_instr_defs;
    u16 max_stack_elements;
    u16 max_size_of_instr;
    u16 max_comp_elements;
    u16 max_comp_depth;
};

struct ttf_htmx_ext {
    u16 advance_width;
    i16 lsb;
};

struct ttf_hmtx {
    u16 num_metrics;
    struct ttf_htmx_ext* metrics;
    i16* lsbs;
};

#define NUMBER_ASCII_CHARS 94
#define START_ASCII_CHAR ' '

// This will contain all the nessecary information about a TTF font
struct ttf_info {

    // ASCII table lookup with indexes
    u16 ascii_lookup[NUMBER_ASCII_CHARS];
    u16 loca[NUMBER_ASCII_CHARS];

    struct ttf_head head;
    struct ttf_maxp maxp;
    struct ttf_hmtx htmx;
};

// Computes the CRC of a table
static inline u32 ttf_get_crc(const u8* table, u32 size)
{
    u32 crc = 0;

    // We only consider the 4 intergral part of the size
    u32 new_size = align_down(size, 4) >> 2;

    while (new_size--) {
        crc += read_be32(table);
        table += 4;
    }

    // Include adjustment calculation to support all tables. This only computes
    // the CRC adjustment due the the unaligned trailing bytes
    u32 adj = 0;
    u32 trail_bytes = size & 0b11;
    for (u32 i = 0; i < trail_bytes; i++)
        adj |= (*table++ << (8 * (3 - i)));

    return crc + adj;
}

// Searches after the given tag in the font table. It takes in a pointer to the 
// top of the font file and the total size of the file
//
// Returns 0 if the given table was found and the CRC matches. Returns -ECRC if
// the table was found but had the wrong crc. Returns -EBFONT if the table was
// not found
static inline i32 ttf_search_table(const u8* ttf, u32 size, u32 tag,
    struct ttf_table_info* rec)
{
    u16 tables = read_be16(ttf + TTF_NUM_TABLES_OFFSET);
    const u8* ptr = ttf + TTF_TABLE_REC_OFFSET;

    while (tables--) {
        u32 ext_tag = read_be32(ptr);

        if (ext_tag != tag) {
            ptr += TTF_TABLE_REC_SIZE;
            continue;
        }

        u32 crc = read_be32(ptr + TTF_TABLE_CRC_OFFSET);
        rec->off = read_be32(ptr + TTF_TABLE_OFF_OFFSET);
        rec->len = read_be32(ptr + TTF_TABLE_LEN_OFFSET);

        u32 new_crc = ttf_get_crc(ttf + rec->off, rec->len);

        // Wrong CRC is not neseccarily a problem. The head table for example 
        // has a intentonally wrong crc
        if (new_crc != crc)
            return ECRC;
        else
            return 0;
    }
    return -EBFONT;
}

// Checks whether the TTF format is supported by this code. Currently the only
// supported font format is TTF / OTF
static inline u32 ttf_is_supported(const u8* ttf, u32 size)
{
    if (size < 4)
        return 0;

    return (read_be32(ttf) == TTF_HEADER_SFNT_VER);
}

// Parses the cmap table.... Fucking bullshit format!!! The goal is to get the
// glyph IDs of the ASCII characters
//
// Returns 0 if the table was parsed successfully. Returns -EBFONT if there were
// some error with the font format
static inline i32 ttf_parse_cmap(const u8* ttf, u32 size, struct ttf_info* info)
{
    struct ttf_table_info pos;

    i32 err = ttf_search_table(ttf, size, TTF_TAG_CMAP, &pos);
    if (err)
        return err;

    u32 num_tables = read_be16(ttf + pos.off + 2);

    // Jump past the header which is 4 byte
    const u8* table_ptr = ttf + pos.off + 4;

    // Pointer to the unicode subtable
    const u8* uni = NULL;

    for (u32 i = 0; i < num_tables; i++) {
        u16 plat_id = read_be16(table_ptr);
        u16 enc_id = read_be16(table_ptr + 2);
        u32 offset = read_be32(table_ptr + 4);

        // We only support windows and Unicode format
        if ((plat_id == 0 && enc_id >= 0 && enc_id <= 4) ||
            (plat_id == 3 && (enc_id == 0 || enc_id == 1 || enc_id == 10))) {
            
            // We only support format 4
            const void* addr = ttf + pos.off + offset;
            if (read_be16(addr) == 4) {
                uni = addr;
                break;
            }
        }
        table_ptr += 8;
    }

    if (uni == NULL)
        return -ENSUPPORT;
    
    // The segment count is the number of segments times two
    u16 seg_cnt = read_be16(uni + 6);

    // We want the glyph IDs for the ASCII characters
    u16 unicode = (u16)START_ASCII_CHAR;
    for (u32 i = 0; i < NUMBER_ASCII_CHARS; i++) {

        // Search to the right location
        u16 segment = 0;
        u16 start = 0;
        u16 end = 0;
        for (segment = 0; segment < seg_cnt; segment += 2) {

            // Get the start and end of the current segment of unicode chars
            start = read_be16(uni + 14 + segment + 2 + seg_cnt);
            end = read_be16(uni + 14 + segment);

            if (unicode >= start && unicode <= end)
                break;
        }

        // The font does not support all ASCII characters
        if (unicode < start || unicode > end)
            return -EBFONT;


        u16 delta = read_be16(uni + 14 + segment + 2 + 2 * seg_cnt);
        u16 off = read_be16(uni + 14 + segment + 2 + 3 * seg_cnt);

        u16 glyph_id = 0;
        if (off == 0) {
            glyph_id = (unicode + delta) & 0xFFFF;
        } else {

            // Current location in the range offset
            const u8* range_ptr = uni + 14 + segment + 2 + 3 * seg_cnt;
            range_ptr += (unicode - start) * 2;
            range_ptr += off;

            glyph_id = read_be16(range_ptr);

            if (glyph_id)
                glyph_id = (glyph_id + delta) & 0xFFFF;
        }
        info->ascii_lookup[i] = glyph_id;
        unicode++;
    }

    return 0;
}

// Tries to parse the glyphs specified in the unicode table
//
// Returns 0 if the parsing was successful. Returns -EBFONT if there is 
// something wrong with the file format
static inline i32 ttf_parse_glyph(const u8* ttf, u32 size, struct ttf_info* info)
{
    // We will want the location whithin the glyph table. This info is retrieved 
    // from the loca table
    struct ttf_table_info pos;

    // Find the position of the loca table
    i32 err = ttf_search_table(ttf, size, TTF_TAG_LOCA, &pos);
    if (err)
        return err;

    const u8* ptr = ttf + pos.off;

    // We need to know the table format given in the head table
    for (u32 i = 0; i < NUMBER_ASCII_CHARS; i++) {
        if (info->head.index_to_loc_fmt)
            info->loca[i] = read_be32(ptr + info->ascii_lookup[i] * 4);
        else
            info->loca[i] = 2 * read_be16(ptr + info->ascii_lookup[i] * 2);
    }

    // Get the position of the glyf table
    err = ttf_search_table(ttf, size, TTF_TAG_GLYF, &pos);
    if (err)
        return err;
    
    for (u32 i = 'B' - START_ASCII_CHAR; i <= 'B' - START_ASCII_CHAR; i++) {
        ptr = ttf + pos.off + info->loca[i];

        // Figure out the number of contour
        i16 num_contours = (i16)read_be16(ptr);
        if (num_contours < 0)
            continue;
        
        // Make ptr point to the start of the glyph description
        ptr += 10;

        // This pointer will allways point to the start of the contour
        const u8* const cont_ptr = ptr;
        
        // Make the pointer point to the last element of the ptr_of_contour
        ptr += (num_contours - 1) * 2;
        i16 num_pts = read_be16(ptr);

        // Start of the instruction
        ptr += 2;
        u16 inst_len = read_be16(ptr);

        // Skip past the instruction section
        ptr += inst_len;

        // This will allways point to the start of the flags section
        const u8* flag_ptr = ptr;

        // Due to the shitty format we have to parse the points three times
        u32 i = 0;
        for (i = 0; i < num_pts; i++) {
            if (*ptr++ & 0x08)
                i += *(++ptr);
        }
        assert(i == num_pts);

        // Parse the X-table

        print("I %d num_pts %d\n", i, num_pts);


    }

    return 0;
}

// Parses the htmx table. This will contain info about the horizontal metrics.
// This includes the advance width and the horisontal spacing.
//
// Returns 0 if the table was parsed successfully. Returns -EBFONT if there were
// some error with the font format
static inline i32 ttf_parse_hmtx(const u8* ttf, u32 size, struct ttf_info* info)
{
    struct ttf_table_info pos;

    // We need the hhea table to determite how many tables there are in the hmtx
    i32 err = ttf_search_table(ttf, size, TTF_TAG_HHEA, &pos);
    if (err < 0)
        return err;

    print("Offset => %d\n", pos.off);

    info->htmx.num_metrics = read_be16(ttf + pos.off);

    print("Number of metrix %d\n", info->htmx.num_metrics + 2);

    return 0;
}

// This function parses the maxp table. This is used to gain information of the 
// resources required to use this font
//
// Returns 0 if the maxp table was parsed successfully. Returns -EBFONT if there
// where an error with the format
static inline i32 ttf_parse_maxp(const u8* ttf, u32 size, struct ttf_maxp* maxp)
{
    struct ttf_table_info info;
    i32 err = ttf_search_table(ttf, size, TTF_TAG_MAXP, &info);
    if (err)
        return -EBFONT;
    
    // Check the version number
    const u8* ptr = ttf + info.off;
    if (read_be32(ptr + TTF_MAXP_VER_OFF) != 0x00010000)
        return -EBFONT;

    maxp->num_glyphs          = read_be16(ptr + TTF_MAXP_NUM_GLYPH_OFF);
    maxp->max_points          = read_be16(ptr + TTF_MAXP_MAX_POINTS_OFF);
    maxp->max_contors         = read_be16(ptr + TTF_MAXP_MAX_COUNTORS_OFF);
    maxp->max_comp_points     = read_be16(ptr + TTF_MAXP_MAX_COMP_POINTS_OFF);
    maxp->max_comp_contours   = read_be16(ptr + TTF_MAXP_MAX_COMP_CONTOURS_OFF);
    maxp->max_zones           = read_be16(ptr + TTF_MAXP_MAX_ZONES_OFF);
    maxp->max_twilight_points = read_be16(ptr + TTF_MAXP_MAX_TWILIGHT_OFF);
    maxp->max_storage         = read_be16(ptr + TTF_MAXP_MAX_STORAGE_OFF);
    maxp->max_func_defs       = read_be16(ptr + TTF_MAXP_MAX_FUNC_DEF_OFF);
    maxp->max_instr_defs      = read_be16(ptr + TTF_MAXP_MAX_INSTR_DEF_OFF);
    maxp->max_stack_elements  = read_be16(ptr + TTF_MAXP_MAX_STACK_ELEM_OFF);
    maxp->max_size_of_instr   = read_be16(ptr + TTF_MAXP_MAX_SIZE_INSTR_OFF);
    maxp->max_comp_elements   = read_be16(ptr + TTF_MAXP_MAX_COMP_ELEM_OFF);
    maxp->max_comp_depth      = read_be16(ptr + TTF_MAXP_MAX_COMP_DEPTH_OFF);

    return 0;
}


// This function parses the head table in the font file. The head table gives 
// global information about the font
//
// Returns 0 if the head table is parser sucessfully and -ENFONT if there
// were an errorfi
static inline i32 ttf_parse_head(const u8* ttf, u32 size, struct ttf_head* head)
{
    struct ttf_table_info info;
    i32 err = ttf_search_table(ttf, size, TTF_TAG_HEAD, &info);
    if (err < 0)
        return err;
    
    const u8* ptr = ttf + info.off;
    if (read_be32(ptr + TTF_HEAD_MAGIC_OFF) != TTF_HEAD_MAGIC)
        return -EBFONT;

    head->flags            = read_be16(ptr + TTF_HEAD_FLAGS_OFF);
    head->units_per_em     = read_be16(ptr + TTF_HEAD_UNIT_PM_OFF);
    head->mac_style        = read_be16(ptr + TTF_HEAD_MAC_STYLE_OFF);
    head->srp              = read_be16(ptr + TTF_HEAD_SRP_OFF);

    head->dir_hint         = (i16)read_be16(ptr + TTF_HEAD_FONT_DIR_OFF);
    head->index_to_loc_fmt = (i16)read_be16(ptr + TTF_HEAD_INDEX_TO_LOC_OFF);
    head->x_min            = (i16)read_le16(ptr + TTF_HEAD_X_MIN_OFF);
    head->y_min            = (i16)read_le16(ptr + TTF_HEAD_Y_MIN_OFF);
    head->x_max            = (i16)read_le16(ptr + TTF_HEAD_X_MAX_OFF);
    head->y_max            = (i16)read_le16(ptr + TTF_HEAD_Y_MAX_OFF);
    
    return 0;
}

// Parses a TTF file
//
// Returns 0 if the TrueType font was parsed successfully. Returns -ENSUPPORT if
// the file format is not supported. Returns -EBFONT in case of a bad font file.
i32 parse_ttf(const u8* ttf, u32 size)
{
    if (!ttf_is_supported(ttf, size))
        return -ENSUPPORT;
    
    print("TTF is supported\n");

    struct ttf_info* info = kmalloc(sizeof(struct ttf_info));

    i32 err = ttf_parse_head(ttf, size, &info->head);
    if (err)
        return err;
    
    print("Units per em: %d\n", info->head.units_per_em);

    err = ttf_parse_maxp(ttf, size, &info->maxp);
    if (err)
        return err;
    
    err = ttf_parse_hmtx(ttf, size, info);
    if (err)
        return err;

    print("Num => %d\n", info->maxp.num_glyphs);

    // The following part uses only the ASCII glyphs. This is going to save
    // a lot of RAM
    err = ttf_parse_cmap(ttf, size, info);
    if (err)
        return err;

    // Now we will use the unicode to id mapping to figure out the glyph points
    err = ttf_parse_glyph(ttf, size, info);
    if (err)
        return err;

    struct screenbuffer* fb = lcd_get_new_screenbuffer(1);

    struct rgba (*b)[800] = fb->buffer;
    for (u32 i = 0; i < 300; i++) {
        b[60][60 + i] = (struct rgba){.a = 0xFF, .b = 0xFF, .r = 0, .g = 0};
    }
    lcd_switch_screenbuffer(1);


    return 0;
}

// Testing thread for the TTF parser
i32 ttf_thread(void* arg)
{
    syscall_thread_sleep(200);
    print("Starting thread\n");

    struct file* dir = dir_open("/sda2/fonts");

    if (!dir)
        panic("Cannot open file\n");

    struct file_info* info = kmalloc(sizeof(struct file_info));
    while (1) {
        i32 err = dir_read(dir, info);

        if (err)
            break;
        
        file_print(info);

        err = get_next_valid_entry(dir);

        if (err)
            break;
    }

    struct file* file = file_open("/sda2/fonts/karla.ttf", FILE_R);

    u8* font = kmalloc(50000);

    u32 cnt = 0;
    u32 err = file_read(file, font, 50000, &cnt);

    if (err)
        panic("Error");
    
    print("Bytes read => %d\n", cnt);
    parse_ttf(font, cnt);
    
    return 0;
}
