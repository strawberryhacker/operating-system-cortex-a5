/// Copyright (C) strawberryhacker

#include <cinnamon/fat.h>
#include <cinnamon/kmalloc.h>
#include <cinnamon/panic.h>
#include <cinnamon/mem.h>
#include <cinnamon/string.h>
#include <cinnamon/disk.h>

struct file_info_short {
    u32 cluster;
    u8 attr;
};

/// This checks the FAT header signature at the end of the BPB. It returns 1
/// if the signature is right
static inline u32 check_fat_signature(const u8* bpb)
{
    if (bpb[510] == 0x55 && bpb[511] == 0xAA) {
        return 1;
    } else {
        return 0;
    }
}

/// Static fucntions
/// Remove this
static void memdump(const void* mem, u32 size, u32 col, u8 hex)
{
    const u8* src = (const u8 *)mem;
    u32 i = 0;
    while (size--) {
        if (hex) {
            print("%02x ", src[i]);
        } else {
            print("%c", src[i]);
        }

        if ((++i % col) == 0) {
            print("\n");
        }
    }
}

/// Read a little-endian half-word pointed to by ptr
u16 read_le16(const void* ptr)
{
    const u8* src = (const u8 *)ptr;
    u16 val = 0;
    
    val |= src[0] << 0;
    val |= src[1] << 8;

    return val;
}

/// Read a little-endian word pointed to by ptr
u32 read_le32(const void* ptr)
{
    const u8* src = (const u8 *)ptr;
    u32 val = 0;

    val |= src[0] << 0;
    val |= src[1] << 8;
    val |= src[2] << 16;
    val |= src[3] << 24;

    return val;
}

/// Checks if the BPB given hold a valid FAT file system
static inline u32 is_fat(const u8* bpb)
{
    if (mem_cmp(bpb + BPB_16_FSTYPE, "FAT", 3)) {
        return 1;
    }
    if (mem_cmp(bpb + BPB_32_FSTYPE, "FAT", 3)) {
        return 1;
    }
    return 0;
}

/// Checks if the BPB belongs to a FAT32 files system
static u32 is_fat32(const u8* bpb)
{
    // Check if it is even a FAT file system
    if (!is_fat(bpb)) {
        return 0;
    }

    // If there is any root clusters in the file system this is not FAT32
    if (read_le16(bpb + BPB_ROOT_ENT_CNT) != 0) {
        return 0;
    }

    // Determite the FAT system type by counting the number of cluster
    u32 fat_pages;
    if (read_le16(bpb + BPB_FAT_SIZE_16)) {
        fat_pages = read_le16(bpb + BPB_FAT_SIZE_16);
    } else {
        fat_pages = read_le32(bpb + BPB_32_FAT_SIZE);
    }

    // Number of total pages
    u32 tot_pages;
    if (read_le16(bpb + BPB_TOT_SECT_16)) {
        tot_pages = read_le16(bpb + BPB_TOT_SECT_16);
    } else {
        tot_pages = read_le32(bpb + BPB_TOT_SECT_32);
    }

    // Number of data pages
    u32 data_pages = tot_pages - read_le16(bpb + BPB_RSVD_CNT) - 
        (fat_pages * bpb[BPB_NUM_FATS]);

    data_pages /= bpb[BPB_CLUSTER_SIZE];

    // Check which file system it is
    if (data_pages >= 65525) {
        return 1;
    } else {
        return 0;
    }
}

/// This converts a order to a bitmask. For example 3 would return 0b111
static u32 fat_order_to_mask(u32 order)
{
    u32 res = 0;
    for (u32 i = 0; i < order; i++) {
        res |= (1 << i);
    }
    return res;
}

/// Mounts the FAT file system by fillng in the FAT structure elements
static void fat_mount(struct partition* part, struct fat* fat, u8* bpb)
{
    u32 pages_bytes = read_le16(bpb + BPB_SECTOR_SIZE);
    u32 cluster_pages = bpb[BPB_CLUSTER_SIZE];

    // Page size
    fat->page_order = __builtin_ctz(pages_bytes);
    fat->page_mask = fat_order_to_mask(fat->page_order);

    // Cluster size
    fat->clust_order = __builtin_ctz(cluster_pages);
    fat->clust_mask = fat_order_to_mask(fat->clust_order);

    // This describes the page size in number of entries. This is to accelerate
    // FAT table look ups
    fat->fat_ent_order = fat->page_order - 2;
    fat->fat_ent_mask = fat_order_to_mask(fat->fat_ent_order);


    // Figure out the number of FATs
    fat->fats = bpb[BPB_NUM_FATS];

    // Figure out the global offsets
    fat->glob_page = part->start_lba;
    fat->fat_glob_page = fat->glob_page + read_le16(bpb + BPB_RSVD_CNT);
    fat->root_glob_page = fat->fat_glob_page +
        (fat->fats * read_le32(bpb + BPB_32_FAT_SIZE));

    // Misc offsets
    fat->info_glob_page = fat->glob_page + read_le16(bpb + BPB_32_FSINFO);
    fat->root_clust_num = read_le32(bpb + BPB_32_ROOT_CLUST);
    
    // Add the short FAT name
    string_add_name(fat->label, (char *)bpb + BPB_32_VOL_LABEL,
        BPB_32_VOL_LABEL_SIZE);
}

/// Called by the disk interface and it is going to probe for a FAT32 file 
/// file system on the partition
u32 fat_mount_partition(struct partition* part)
{
    u8* buf = kmalloc(512);
    u32 status = part->parent_disk->read(part->parent_disk, 
        part->start_lba, 1, buf);

    if (!status) {
        panic("Cant read FAT header");
    }

    // Check the FAT header signature is right
    if (!check_fat_signature(buf)) {
        return 0;
    }

    // Check if this is a FAT32 file system
    if (!is_fat32(buf)) {
        return 0;
    }

    // Allocate the FAT32 file system
    struct fat* fat = kmalloc(sizeof(struct fat));
    part->fs = fat;

    // Mount the file system
    fat_mount(part, fat, buf);

    // Free the BPB buffer
    kfree(buf);

    return 1;
}

/// Returns if the entry is a LFN entry
static u32 fat_is_lfn(u8* dir_entry)
{
    u32 attr = dir_entry[LFN_ATTR];
    if (attr == ATTR_LFN) {
        return 1;
    }
    return 0;
}

/// Converts a file to an LBA
static inline u32 fat_file_to_page(struct partition* part, struct file* file)
{
    return file->page + part->fs->root_glob_page;
}

/// Caches a file allocation table page
static inline u32 fat_cache_fat_page(struct partition* part, struct file* file,
    u32 glob_page)
{
    if (glob_page != file->fat_cache_glob_page) {
        
        // Cache another FAT page
        struct disk* disk = part->parent_disk;
        u32 status = disk->read(disk, glob_page, 1, (u8 *)file->fat_cache);
        if (!status) {
            panic("Error");
            return 0;
        }
        file->fat_cache_glob_page = glob_page;
    }
    return FAT_OK;
}

/// Follows the cluster chain of a file, incrementing the cluster number by 
/// count
static inline u8 fat_follow_cluster_chain(struct partition* part,
    struct file* file, u32* clust_ptr, u32 count)
{
    u32 clust = *clust_ptr;
    struct fat* fat = part->fs;

    for (u32 i = 0; i < count; i++) {

        // We need to find the global page and the FAT entry offset whithin a
        // page
        u32 page = fat->fat_glob_page + (clust >> fat->fat_ent_order);
        u32 entry = clust & fat->fat_ent_mask;

        if (fat_cache_fat_page(part, file, page) != FAT_OK) {
            panic("Error");
            return FAT_ERROR;
        }

        // The FAT cache is holding the right sector
        u32 fat_entry = file->fat_cache[entry];

        // Check if the entry is valid. The cluster can either be bad, free or 
        // allocated or EOCC (end of cluster chain)
        if (fat_entry >= 0xFFFFFF8 && fat_entry <= 0xFFFFFFF) {

            // EOCC
            return FAT_EOCC;
        }

        // Find the next cluster in the chain if not free or bad
        if (fat_entry >= 0x0000002 && fat_entry <= 0xFFFFFEF) {
            clust = fat_entry;
        } else {
            panic("FAT error during cluster walk - bad or free cluster");
        }
    }

    // Write the new cluster back to the cluster pointer. This will probably be
    // an entry in the file structure
    *clust_ptr = clust;

    return FAT_OK;
}

/// Fetches an LBA sector into file cache. Returning a FAT status
static u8 fat_cache(struct partition* part, struct file* file)
{
    /// Find the new LBA
    u32 page = fat_file_to_page(part, file);

    if (page == file->cache_glob_page) {
        return FAT_OK;
    }

    // The LBA is not in the cache
    if (file->cache_dirty) {
        panic("Go implement file -> part -> disk -> mmc -> write");
        file->cache_dirty = 0;
    }

    // Fetch the new page into the cache
    struct disk* disk = part->parent_disk;
    
    u32 status = disk->read(disk, page, 1, file->cache);
    if (!status) {
        print("Disk read error\n");
        return FAT_ERROR;
    }

    // Update the new LBA in cache
    file->cache_glob_page = page;
    return FAT_OK;
}

/// Increments the rw offset by a number of bytes and caches any new sector
static u32 fat_inc_file_offset(struct partition* part, struct file* file,
    u32 bytes)
{
    struct fat* fat = part->fs;

    file->file_offset += bytes;
    file->page_offset += bytes;

    // Check if we have to resolve the page offset in case of overflow
    if (file->page_offset & ~fat->page_mask) {

        // Find how many pages we have to increment
        u32 page_inc = file->page_offset >> fat->page_order;
        file->page_offset &= fat->page_mask;

        // The page increment might overflow the cluster number
        u32 curr_cluster = file->page >> fat->clust_order;
        u32 clust_jump = ((file->page + page_inc) >> fat->clust_order) -
            curr_cluster;

        // If cluster jump is non-zero we have to follow the cluster chain in
        // the file allocation table
        if (clust_jump) {
            u32 status = fat_follow_cluster_chain(part, file, &curr_cluster,
                clust_jump);

            // The cluster chain walk might fail because of FAT cache update
            if (status == FAT_ERROR) {
                panic("Error");
            }

            // If we have a cluster overflow, the page number if now wrong
            file->page = ((curr_cluster - fat->root_clust_num) << fat->clust_order) + 
                ((file->page + page_inc) & fat->clust_mask);

        } else {
            file->page += page_inc;
        }

        if (fat_cache(part, file) != FAT_OK) {
            panic("ok");
        }
    }
    return 1;
}

static inline u32 fat_jump_entries(struct partition* part, struct file* dir, 
    u32 jump) 
{
    return fat_inc_file_offset(part, dir, jump * 32);
}

/// Takes in a dir pointer and increment the pointer so that it is pointing to 
/// the next valid entry. This is returns a FAT status code
static u8 fat_next_entry(struct partition* part, struct file* dir)
{
    // If the current one is a LFN iterate untill the corresponding SFN
    if (dir->cache[dir->page_offset + SFN_ATTR] == ATTR_LFN) {
        for (u32 i = 0; i < (dir->cache[dir->page_offset] & LFN_SEQ_MSK); i++) {
            if (!fat_jump_entries(part, dir, 1)) {
                return FAT_ERROR;
            }
        }
    }

    // The dir will point to a SFN
    while (1) {
        if (!fat_jump_entries(part, dir, 1)) {
            return FAT_ERROR;
        }
        u8 tmp = dir->cache[dir->page_offset];

        // Check for end EOD marker
        if (tmp == 0x00) {
            return FAT_EOD;
        }

        // Check for free entry
        if (tmp != 0x05 && tmp != 0x2E && tmp != 0xE5) {
            break;
        }
    }

    return FAT_OK;
}

/// This takes in a path starting with /. It takes in the pointer to the
/// previous fragment (or / in case of the first one). This will return a pointer
/// to the next fragment if existing and return its size
static u32 fat_get_next_path_frag(const char* path, u32 size, const char** frag)
{
    const char* curr_frag = *frag;
    const char* end = path + size;

    // Iterate unil the start of the new fragment
    while ((curr_frag < end) && (*curr_frag != '/')) {
        curr_frag++;
    }

    // Go past the slash
    curr_frag++;

    // Check for overflow
    if (curr_frag >= end) {
        return 0;
    }

    *frag = curr_frag;
    u32 len = 0;

    // Iterate unil the start of the new fragment
    while ((curr_frag < end) && (*curr_frag != '/')) {
        curr_frag++;
        len++;
    }

    return len;
}

/// Update the file pointer with a cluster number. Return a FAT status code
static u8 fat_set_cluster(struct partition* part, struct file* file, u32 clust) 
{
    struct fat* fat = part->fs;

    file->page = ((clust - fat->root_clust_num) << fat->clust_order);
    file->page_offset = 0;

    return fat_cache(part, file);
}

/// Sets the file object (dir) point to the root directory
static u8 fat_file_set_root(struct partition* part, struct file* dir)
{
    struct fat* fat = part->fs;

    dir->file_offset = 0;
    dir->page_offset = 0;
    dir->page = 0;

    dir->cache_dirty = 0;
    dir->cache_glob_page = 0;
    dir->fat_cache_glob_page = 0;
    dir->lfn_offset = 0;

    u32 status = fat_cache(part, dir);

    if (status != FAT_OK) {
        panic("Caching failed");
        return 0;
    }

    return 1;
}

#define LFN_INDEX_SIZE 3

/// Indexes for LFN name entries
struct {
    u8 size;
    u8 offset;
} const lfn_index[LFN_INDEX_SIZE] = {
    { .size = 5, .offset = 1 },
    { .size = 6, .offset = 14 },
    { .size = 2, .offset = 28 }
};

/// Testing
static void print_entry(struct partition* part, struct file* dir)
{
    print(GREEN);
    u8* entry_ptr = dir->cache + dir->page_offset;

    u8 attr = entry_ptr[SFN_ATTR];
    if (attr == ATTR_LFN) {
        print("LFN entry\n");

        for (u32 i = 0; i < LFN_INDEX_SIZE; i++) {
            for (u32 j = 0; j < lfn_index[i].size; j++) {
                u8 off = lfn_index[i].offset;
                print("%c", entry_ptr[off + j*2]);
            }
        }   
        print("\n");

    } else {
        print("SFN entry\n");
        
        // 8.3
        print("%8s.%3s\n", entry_ptr + SFN_NAME, entry_ptr + SFN_NAME + 8);
    }
    print(NORMAL);
}

/// Converts a char to uppercase
static inline char fat_to_upper(char a)
{
    if (a <= 'z' && a >= 'a') {
        return a - 32;
    }
    return '#';
}

/// Converts a char to uppercase
static inline char fat_to_lower(char a)
{
    if (a <= 'Z' && a >= 'A') {
        return a + 32;
    }
    return '#';
}

/// Compares two char case insensitive
static inline u8 fat_cmp_char_unified(char a, char b)
{
    a = fat_to_upper(a);
    b = fat_to_upper(b);

    return (u8)(a == b);
}

#define NUM_ILLEGAL_CHAR_SFN 16

const u8 illegal_sfn_chars[NUM_ILLEGAL_CHAR_SFN] = {
    0x22, 0x2A, 0x2B, 0x2C, 0x2E, 0x2F, 0x3A, 0x3B,
    0x3C, 0x3D, 0x3E, 0x3F, 0x5B, 0x5C, 0x5D, 0x7C 
};

/// Returns if the given SFN character is valid
static u8 fat_sfn_char_valid(char sfn_char)
{
    // Check if the SFN char is valid
    for (u32 i = 0; i < NUM_ILLEGAL_CHAR_SFN; i++) {
        if (sfn_char == illegal_sfn_chars[i]) {
            return 0;
        }
    }

    // It can be lower than space
    if (sfn_char <= 0x20) {
        return 0;
    }

    return 1;
}

/// Compares a SFN char agains the file name char and returns 1 if they match
static inline u8 fat_sfn_cmp_char(char sfn_char, char file_char)
{
    // Check if the SFN char is valid
    for (u32 i = 0; i < NUM_ILLEGAL_CHAR_SFN; i++) {
        if (sfn_char == illegal_sfn_chars[i]) {
            return 0;
        }
    }

    // It can be lower than space
    if (sfn_char <= 0x20) {
        return 0;
    }

    // Convert the file char to uppercase. The spec requires all SFN chars to be
    // uppercase
    return (u8)(fat_to_upper(file_char) == sfn_char);
}

/// Converts a SFN 8.3 name to a normal filename. The buffer should at least be
/// 11 + 1 + 1 bytes long. 11 character + 1 dot + NULL termination. This will
/// add the NULL terminating character
static void fat_sfn_to_file_name(const char* sfn, char* buffer)
{
    u8 i;
    u32 j;

    // Get the first part
    for (i = 0; i < 8; i++) {
        if (sfn[i] == ' ') {
            break;
        }
        buffer[i] = fat_to_lower(sfn[i]);
    }

    // Get the extension
    u8 dot = 0;
    for (j = 0; j < 3; j++) {
        u8 tmp = sfn[8 + j];
        if (tmp != ' ') {
            dot = 1;
        } else {
            break;
        }
        buffer[i + j + 1] = fat_to_lower(tmp);
    }
    buffer[i + j + dot] = 0x00;

    // Check if we have to update the dot
    if (dot) {
        buffer[i] = '.';
    }
}

/// Converts a filename to a SFN 8.2 name
static u8 fat_file_name_to_sfn(const char* file_name, u32 len, u8* sfn)
{
    const char* file_tmp = file_name;
    u32 i = 0;
    for (;(i < 8) && (i < len); i++) {

        // File name done
        if (*file_name == '.') {
            break;
        }

        // Check for a valid character
        if (!fat_sfn_char_valid(*file_name)) {
            return 0;
        }
       
        *sfn++ = fat_to_upper(*file_name++);
    }

    // Check for dot and padd with spaces
    if (*file_name != '.') {
        if (len <=  i + 1) {
            for (;i < 11; i++) {
                *sfn++ = ' ';
            }
            return 1;
        } else {
            return 0;
        }
    }

    // We have a dot
    file_name++;

    // Padd the rest of the 8 file name
    for (;i < 8; i++) {
        *sfn++ = ' ';
    }

    // Add the extension 3 chars
    u8 off = file_name - file_tmp;
    for (; i < 11 && off < len - 1; i++, off++) {
        *sfn++ = fat_to_upper(*file_name++);
    }

    // Check for dot and padd with spaces
    if (i == 11 && off < len - 1) {
        return 0;
    }

    // Padd the rest of the extension
    for (;i < 11; i++) {
        *sfn++ = ' ';
    }
    return 1;
}

/// Compared a SFN entry 8.3 file name with the given string
static u8 fat_sfn_compare(const char* sfn, const char* file_name, u32 len)
{
    // Storing thew full file name in the SFN buffer
    u8 sfn_buffer[12];
    if (!fat_file_name_to_sfn(file_name, len, sfn_buffer)) {
        return 0;
    }

    if (!mem_cmp(sfn_buffer, sfn, 11)) {
        return 0;
    }
    return 1;
}

/// Compared a fragment of a string agains the filename at a given offset. 
/// The entire fragment has to exist in the filefile in order for this 
/// function to return 1
static u8 fat_lfn_cmp_frag(const char* file_name, u32 len, const char* frag,
    u32 frag_len, u32 offset)
{

    print(YELLOW);
    print("Searching for %*s in %*s at offset %d\n", frag_len, frag, len, file_name, offset);
    print(NORMAL);

    if (offset > len - 1) {
        return 0;
    }

    len -= offset;
    file_name += offset;

    u32 i = 0;
    while ((i < len) && (i < frag_len)) {
        if (*file_name++ != *frag++) {
            return 0;
        }
        i++;
    }

    // Check if the entire fragment exist in the file path
    if (i < frag_len) {
        return 0;
    }
    return 1;
}

/// Gets the LFN CRC from the SFN 8.3 file name
static u8 fat_get_sfn_crc(const u8* sfn)
{
   u8 crc = 0;
   for (u8 i = 11; i; i--)
      crc = ((crc & 1) << 7) + (crc >> 1) + *sfn++;

   return crc;
}

/// Converts the USC LFN characters to ASCII and returns them in a 13 bytes
/// LFN buffer
static u8 fat_get_lfn_name(const char* lfn, char* lfn_buffer)
{
    u8 cnt = 0;
    for (u32 i = 0; i < LFN_INDEX_SIZE; i++) {
        for (u32 j = 0; j < lfn_index[i].size; j++) {
            u8 offset = lfn_index[i].offset;
            u8 tmp = lfn[offset + j * 2];

            if (!tmp) {
                return cnt;
            }

            cnt++;
            *lfn_buffer++ = tmp;
        }
    }
    return cnt;
}

/// Compares a directory entry agains a given file name. This will move the file
/// pointer so that it points to the SFN entry of the current block
static u32 fat_compare_entry(struct partition* part, struct file* dir, 
    const char* file_name, u32 len)
{
    // Two cases - either the dir is pointing to a SFN or a chain of LFN
    // followed by a SFN
    const u8* entry = dir->cache + dir->page_offset;

    if (entry[SFN_ATTR] != ATTR_LFN) {

        // We are going to do one single SFN compare
        if (!fat_sfn_compare((char *)entry, file_name, len)) {
            return 0;
        } else {
            return 1;
        }
    }

    // The dir should point to the first LFN entry. In this case bit 6 is set
    // in the LFN sequence number
    if (!(entry[LFN_SEQ] & (1 << 6))) {
        panic("Error in sequence number\n");
    }

    // Compare the LFN file name
    u8 seq;
    u8 crc = entry[LFN_CRC];
    char lfn[13];
    do {
        // Update the sequnce number
        seq = entry[LFN_SEQ] & LFN_SEQ_MSK;

        // Read the LFN name
        u32 lfn_size = fat_get_lfn_name((char *)entry, lfn);

        // Compare the LFN fragment and fix the dir pointer in case of a 
        // file mismatch
        if (!fat_lfn_cmp_frag(file_name, len, lfn, lfn_size, 13 * (seq - 1))) {
            fat_jump_entries(part, dir, seq);
            return 0;
        }    

        // Move the directory entry window
        fat_jump_entries(part, dir, 1);

        // Update the entry
        entry = dir->cache + dir->page_offset;

    } while (seq != 1);

    // We should at this point have dir pointing to the SFN following the last
    // LFN entry. The last thing to do is to check if the LFN chain is matching
    // the 8.3 SFN file name
    if (crc != fat_get_sfn_crc(entry)) {
        panic("CRC error");
    }

    return 1;
}

/// Compares a directory entry agains a given file name. This will move the file
/// pointer so that it points to the SFN entry of the current block
static u8 fat_get_lfn_full_name(struct partition* part, struct file* dir, 
    char* buffer)
{
    u32 cnt = 0;
    u8* entry = dir->cache + dir->page_offset;

    // The dir should point to the first LFN entry. In this case bit 6 is set
    // in the LFN sequence number
    if (!(entry[LFN_SEQ] & (1 << 6))) {
        return 0;
    }

    // Compare the LFN file name
    u8 seq;
    u8 crc = entry[LFN_CRC];
    do {
        // Update the sequnce number
        seq = entry[LFN_SEQ] & LFN_SEQ_MSK;

        // Read the LFN name
        cnt += fat_get_lfn_name((char *)entry, buffer + 13 * (seq - 1));

        // Move the directory entry window
        if (!fat_jump_entries(part, dir, 1)) {
            return 0;
        }

        // Update the entry
        entry = dir->cache + dir->page_offset;

    } while (seq != 1);

    // We should at this point have dir pointing to the SFN following the last
    // LFN entry. The last thing to do is to check if the LFN chain is matching
    // the 8.3 SFN file name
    if (crc != fat_get_sfn_crc(entry)) {
        panic("CRC error");
        return 0;
    }

    return cnt;
}

/// This will take on the root directory and search for the file_name in that 
/// directory. The file pointer will point to either the EOF or the SFN entry
/// of the match
static u32 fat_dir_search(struct partition* part, struct file* dir,
    const char* file_name, u32 len, struct file_info_short* info)
{
    print("Searching for file %*s\n", len, file_name);

    while (1) {

        //print_entry(part, dir);

        // Search after filename in the current dir entry
        if (fat_compare_entry(part, dir, file_name, len)) {
            // Check the LFN CRC agains the SFN 8.3 file name
            print(RED "Match\n" NORMAL);
            return 1;
        }

        // Skip any empty directory entries
        while (1) {
            fat_jump_entries(part, dir, 1);

            u8 tmp = dir->cache[dir->page_offset];

            // Entry is free and no subsequenct entry is in use
            if (tmp == 0) {
                print("Search complete\n");
                return 0;
            }

            // Check if the new entry is not free
            if (tmp != 0x05 && tmp != 0x2E && tmp != 0xE5) {
                break;
            }
            print("Free entry\n");
        }
    }
}

/// This takes in a directory entry and return the cluster number
static u32 fat_dir_ent_to_clust(const u8* dir_ent)
{
    u32 cluster = 0;

    cluster |= read_le16(dir_ent + SFN_CLUSTL);
    cluster |= read_le16(dir_ent + SFN_CLUSTH) << 16;

    return cluster;
}

/// Takes in a pointer to a current directory, and it will follow the path given
/// by path and update the file (aka dir) object
static u32 fat_follow_path(struct partition* part, struct file* dir,
    const char* path, u32 size, u8 dir_only)
{
    const char* curr_frag = path;

    while (1) {
        u32 len = fat_get_next_path_frag(path, size, &curr_frag);

        if (!len) {
            break;
        }

        // This has a directory pointed to by file and a file name pointed to 
        // by curr_frag and len
        struct file_info_short info;
        u32 status = fat_dir_search(part, dir, curr_frag, len, &info);

        if (!status) {
            return 0;
        }

        if (dir_only) {
            u8 attr = dir->cache[dir->page_offset + SFN_ATTR];
            if ((attr & ATTR_DIR) == 0) {
                return 0;
            }
        }

        // We have a match in the path
        u32 clust = fat_dir_ent_to_clust(dir->cache + dir->page_offset);

        // Reset the file offset - really?
        dir->file_offset = 0;

        // This is the point where we move the file pointer
        if (fat_set_cluster(part, dir, clust) != FAT_OK) {
            panic("Error");
        }

        print("Going into directory - cluster %d\n", clust);
    }

    return 1;
}

/// This functions will take in a pointer to a directory. If this is a SFN it 
/// will remain unchanged. The buffer should be at least 256 bytes. 
///
/// If the dir point to a LFN entry is will parse the file name and move the dir
/// pointer to the SFN entry following that. If the user want to save the file
/// he can call the internal file save
///
/// THis returns 1 if the buffer is written. And zero if the buffer is not. In 
/// this case the dir is not changed
static u8 fat_get_entry_name(struct partition* part, struct file* dir,
    char* buffer)
{
    // Check if the new entry is in use
    u8 tmp = dir->cache[dir->page_offset];

    if (tmp == 0x2E) {
        return FAT_DOT;
    }
    if (tmp == 0x00) {
        return FAT_EOD;
    }
    if (tmp == 0x05 || tmp == 0xE5) {
        return FAT_ERROR;
    }

    // Check whether we have a LFN or a SFN
    if (dir->cache[dir->page_offset + SFN_ATTR] != ATTR_LFN) {

        // SFN
        fat_sfn_to_file_name((char *)(dir->cache + dir->page_offset), buffer);

        return FAT_OK;
    }

    // We have a LFN entry
    u8 cnt = fat_get_lfn_full_name(part, dir, buffer);

    // Fix the NULL character
    buffer[cnt] = 0;

    return (cnt == 0) ? FAT_ERROR : FAT_OK;
}

/// Iternal structure for position saving when reading directory entries
struct file_ptr {
    u32 file_offset;
    u32 page_offset;
    u32 page;
};

/// Saves the internal position of a file into the lightweight file pointer
static void _fat_file_save(const struct file* file, struct file_ptr* ptr)
{
    ptr->file_offset = file->file_offset;
    ptr->page_offset = file->page_offset;
    ptr->page = file->page;
}

/// Restores the internal position of a file from the lightweight file pointer
static u8 _fat_file_restore(struct partition* part, struct file* file,
    struct file_ptr* ptr)
{
    file->page = ptr->page;
    file->page_offset = ptr->page_offset;
    file->file_offset = ptr->file_offset;

    return fat_cache(part, file);
}

/// Update the file time structure from the 16-bit time variable in the SFN
static void fat_get_time(u16 time, struct file_time* time_ptr)
{
    time_ptr->hour = time >> 11;
    time_ptr->min = (time >> 5) & 0b111111;
    time_ptr->sec = (time & 0b11111) * 2;
}

/// Update the file data structure from the 16 bit data variable in the SFN
static void fat_get_date(u16 date, struct file_date* date_ptr)
{
    date_ptr->year = 1980 + ((date >> 9) & 0b1111111);
    date_ptr->month = (date >> 5) & 0b1111;
    date_ptr->day = date & 0b11111;
}

/// Takes in a pointer to a SFN entry and updates the file info structure
static void fat_get_sfn_info(const u8* sfn, struct file_info* info)
{
    info->attr = sfn[SFN_ATTR];
    info->size = read_le32(sfn + SFN_FILE_SIZE);

    // Get the time
    fat_get_time(read_le16(sfn + SFN_CTIME), &info->create_time);
    fat_get_time(read_le16(sfn + SFN_WTIME), &info->write_time);

    // Get the date
    fat_get_date(read_le16(sfn + SFN_CDATE), &info->create_date);
    fat_get_date(read_le16(sfn + SFN_WDATE), &info->write_date);
    fat_get_date(read_le16(sfn + SFN_ADATE), &info->access_date);
}

/// Public API
u8 fat_open_dir(struct partition* part, struct file* dir, const char* path, 
    u32 size)
{
    // In case clear the cache dirty
    dir->cache_dirty = 0;

    // Start at the root directory
    if (!fat_file_set_root(part, dir)) {
        print("Error");
    }

    u32 status = fat_follow_path(part, dir, path, size, 1);
    if (!status) {
        return 0;
    }

    return 1;
}

/// Takes in a directory pointer and reads the directory entry
u8 fat_dir_read(struct partition* part, struct file* dir,
    struct file_info* info)
{
    struct file_ptr ptr;
    _fat_file_save(dir, &ptr);

    // Grab the entry name and increase the dir pointer in case of LFN
    u8 status = fat_get_entry_name(part, dir, info->name);
    if (status != FAT_OK) {
        return status;
    }

    // Get the SFN info
    fat_get_sfn_info(dir->cache + dir->page_offset, info);

    if (_fat_file_restore(part, dir, &ptr) != FAT_OK) {
        panic("Cant restore file pointer");
    }

    return FAT_OK;
}

#define NW 20
#define DW 16
#define TW 20

static void file_header(void)
{
    
    print("%-*s %-*s %-*s\n", NW, "Name", DW, "Date modified", TW, "Type");
    print("-----------------------------------------------\n");
}

static void file_print(struct file_info* info)
{
    // File name
    print("%-*s ", NW, info->name);

    // Date
    print("%02d/%02d/%04d %02d:%02d ", 
        info->write_date.day, info->write_date.month, info->write_date.year,
        info->write_time.hour, info->write_time.min);
    
    if (info->attr & ATTR_DIR) {
        print("%-*s", TW, "folder");
    } else {
        print("%-*s", TW, "file");
    }
    print("\n");
}

void fat_test(struct disk* disk)
{
    print("FAT test functions\n");

    struct partition* part = &disk->partitions[1];
    struct file* dir = kmalloc(sizeof(struct file));

    // hello this is small test to check if LFN are working
    // thisisatesttoseeifthisisworking
    char str[] = "/drivers/board/";
    u8 status = fat_open_dir(part, dir, str, sizeof(str) - 1);

    struct file_info* finfo = kmalloc(sizeof(struct file_info));

    print("Reading dir\n");

    
    file_header();
    for (u32 i = 0; i <  25; i++) {
        status = fat_dir_read(part, dir, finfo);

        if (status != FAT_OK && status != FAT_DOT) {
            break;
        }

        if (status != FAT_DOT) {
            file_print(finfo);
        }

        status = fat_next_entry(part, dir);

    }

    print("DONE\n");
}
