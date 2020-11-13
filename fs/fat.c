/// Copyright (C) strawberryhacker

/// Implementation of the FAT32 file system including LFN support

#include <citrus/fat.h>
#include <citrus/kmalloc.h>
#include <citrus/panic.h>
#include <citrus/mem.h>
#include <citrus/string.h>
#include <citrus/disk.h>
#include <citrus/fs.h>
#include <citrus/page_alloc.h>
#include <citrus/error.h>

/// Prints the FAT status code to the serial console
/// REMOVE
static void fat_status(u8 status)
{
    if (status == FAT_OK) {
        return;
    }
    
    if (status & FAT_BAD_PATH) {
        print(" > BAD_PATH\n");
    }
    if (status & FAT_ERROR) {
        print(" > FAT_ERROR\n");
    }
    if (status & FAT_EOF) {
        print(" > EOF\n");
    }
    if (status & FAT_EOCC) {
        print(" > EOCC\n");
    }
    if (status & FAT_DISK_ERROR) {
        print(" > DISK_ERROR\n");
    }
    if (status & FAT_BAD_CLUST) {
        print(" > BAD_CLUST\n");
    }
}


/// This checks the FAT header signature at the end of the BPB page. It returns
/// 1 if the signature is right, 0 is not
static inline u8 fat_signature_ok(const u8* bpb)
{
    return (bpb[510] == 0x55 && bpb[511] == 0xAA);
}


/// Dumps a memory region to the seiral console
/// REMOVE
static void memdump(const void* mem, u32 size, u32 col, u8 hex)
{
    const u8* src = (const u8 *)mem;
    
    for (u32 i = 0; size--;) {
        if (hex) {
            print("%02x ", src[i]);
        } else {
            print("%c", src[i]);
        }

        // New line after col columns
        if ((++i % col) == 0) {
            print("\n");
        }
    }
}


/// Checks if the BPB given hold a valid FAT file system. Returns 1 if the BPB
/// belongs to any FAT 12/16/32 file system
static u8 bpb_contain_valid_fat(const u8* bpb)
{
    if (mem_cmp(bpb + BPB_16_FSTYPE, "FAT", 3)) {
        return 1;
    }
    if (mem_cmp(bpb + BPB_32_FSTYPE, "FAT", 3)) {
        return 1;
    }
    return 0;
}


/// Checks if the BPB contains a valid FAT32 files system
static u8 bpb_contain_fat32(const u8* bpb)
{
    if (!bpb_contain_valid_fat(bpb)) {
        return 0;
    }

    // If there is any root clusters in the file system, it is not a FAT32
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

    u32 tot_pages;
    if (read_le16(bpb + BPB_TOT_SECT_16)) {
        tot_pages = read_le16(bpb + BPB_TOT_SECT_16);
    } else {
        tot_pages = read_le32(bpb + BPB_TOT_SECT_32);
    }

    u32 data_pages = tot_pages - read_le16(bpb + BPB_RSVD_CNT) - 
        (fat_pages * bpb[BPB_NUM_FATS]);

    // This hold the total number of data cluster in the volume
    data_pages /= bpb[BPB_CLUSTER_SIZE];
    
    return (data_pages >= 65525);
}


/// This converts an order to a bitmask. For example 3 would return 0b111. The
/// functions returns a 32 bit mask
static u32 fat_order_to_mask(u8 order)
{
    assert(order < 32);
    
    u32 res = 0;
    for (u32 i = 0; i < order; i++) {
        res <<= 1;
        res |= 1;
    }
    return res;
}


/// Builds the FAT strucure. Cluster, sector and FAT table sizes are converted
/// to bitmasks for fast modulo and floot division. This function assumes the 
/// the presence of a FAT32 file system on the BPB
static void build_fat_struct(struct partition* part, struct fat* fat, u8* bpb)
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
    // FAT table look 
    fat->fat_ent_order = fat->page_order - 2;
    fat->fat_ent_mask = fat_order_to_mask(fat->fat_ent_order);

    // Number of FATs
    fat->fats = bpb[BPB_NUM_FATS];

    // Global offsets
    fat->glob_page = part->start_lba;
    fat->fat_glob_page = fat->glob_page + read_le16(bpb + BPB_RSVD_CNT);
    fat->root_glob_page = fat->fat_glob_page +
        (fat->fats * read_le32(bpb + BPB_32_FAT_SIZE));

    // Misc offsets
    fat->info_glob_page = fat->glob_page + read_le16(bpb + BPB_32_FSINFO);
    fat->root_clust_num = read_le32(bpb + BPB_32_ROOT_CLUST);

    part->fs = fat;
}


/// Returns 1 if the directory entry is a LFN entry
static inline u8 is_lfn(const u8* dir_entry)
{
    return (dir_entry[LFN_ATTR] == ATTR_LFN);
}


/// Converts a file pointer to a page number. This will hold the page on the 
/// disk where the file is pointing. This returns a disk global page
static inline u32 file_get_glob_page(const struct partition* part, 
    const struct file* file)
{
    return file->page + part->fs->root_glob_page;
}


/// Caches the FAT table page pointed to by glob_page
static i8 cache_fat_page(const struct partition* part, struct file* file,
    u32 glob_page)
{
    if (glob_page != file->fat_cache_glob_page) {
        
        // The global page is currently not in the cache so we have to cache it
        const struct disk* disk = part->parent_disk;
        u32 status = disk->read(disk, glob_page, 1, (u8 *)file->fat_cache);
        if (!status) {
            return -EDISK;
        }

        // A new page has been cached 
        file->fat_cache_glob_page = glob_page;
    }
    return 0;
}


/// Returns the FAT status based on a cluster
static i8 get_fat_ent_status(u32 fat_entry)
{
    // If not done; mask the entry
    fat_entry &= FAT_ENTRY_MASK;

    // Check for a normal cluster
    if (fat_entry >= 0x0000002 && fat_entry <= 0xFFFFFEF) {
        return 0;
    }

    // Check for end-of-chain cluster
    if (fat_entry >= 0xFFFFFF8 && fat_entry <= 0xFFFFFFF) {
        return EEOF;
    }

    return -EDISK;
}


/// Takes in a pointer to a cluster number and follows the cluster chain `count`
/// clusters. If successful it updates the cluster. 
///
/// Returns  EEOF if this is the last cluster in the chain
/// Returns -EEOF if the last cluster has occured
/// Returns -EDISK in case of disk error
static i8 follow_cluster_chain(const struct partition* part, struct file* file,
    u32* clust_ptr, u32 count)
{
    i8 err = 0;
    const struct fat* fat = part->fs;

    // Initial cluster value
    u32 clust = *clust_ptr;

    for (u32 i = 0; i < count; i++) {
        u32 ent_page = fat->fat_glob_page + (clust >> fat->fat_ent_order);
        u32 ent_index = clust & fat->fat_ent_mask;

        // Try cache the global FAT table page in the FAT cache
        err = cache_fat_page(part, file, ent_page);
        if (err < 0)
            return err;

        u32 ent = file->fat_cache[ent_index] & FAT_ENTRY_MASK;
        err = get_fat_ent_status(ent);
        // Cluster is bad
        if (err < 0)
            return err;

        // End of file, but we are not done
        if ((err == EEOF) && (i < (count - 1)))
            return -EEOF;

        clust = ent;
    }

    *clust_ptr = clust;
    return err;
}


/// Caches the current file pointer page into the file structure cache
///
/// Returns EDISK in case of disk error
static i8 fat_cache(const struct partition* part, struct file* file)
{
    u32 page = file_get_glob_page(part, file);

    // Page is allready in the cache
    if (page == file->cache_glob_page) {
        return 0;
    }

    if (file->cache_dirty) {
        panic("Disk write implementation missing!");
        file->cache_dirty = 0;
    }

    // Fetch the new page into the cache
    const struct disk* disk = part->parent_disk;
    
    u32 status = disk->read(disk, page, 1, file->cache);
    if (!status) {
        return -EDISK;
    }

    // Update the new LBA in cache
    file->cache_glob_page = page;
    return 0;
}


/// Increments the file pointer by a number of bytes and caches the new page. 
/// 
/// Returns -EEOF if the EOF has been reached without being done
/// Returns -EDISK in case of disk error
static inline u8 fat_inc_file_ptr(const struct partition* part,
    struct file* file, u32 bytes)
{
    i8 err;
    const struct fat* fat = part->fs;

    file->file_offset += bytes;
    file->page_offset += bytes;

    // Page overflow
    if (file->page_offset & ~fat->page_mask) {

        // Find how many pages we have to increment
        u32 page_inc = file->page_offset >> fat->page_order;
        file->page_offset &= fat->page_mask;

        u32 curr_cluster = (file->page >> fat->clust_order) +
            fat->root_clust_num;
        u32 next_cluster = ((file->page + page_inc) >> fat->clust_order) + 
            fat->root_clust_num;

        // Find how many clusters we have to jump
        u32 clust_cnt = next_cluster - curr_cluster;

        if (clust_cnt) {
            err = follow_cluster_chain(part, file, &curr_cluster, clust_cnt);

            // The cluster chain walk might fail because of FAT cache update
            if (err < 0)
                return err;

            // If we have a cluster overflow, the page number might have changed
            file->page = ((file->page + page_inc) & fat->clust_mask) + 
                ((curr_cluster - fat->root_clust_num) << fat->clust_order);

        } else {
            file->page += page_inc;
        }

        // The page has changed so we must cache another page
        err = fat_cache(part, file);
        if (err < 0)
            return err;
    }
    return 0;
}


/// Jumps a number of entries in a directory. This will increment the file
/// pointer with a multiple of 32 bytes
///
/// Returns -EEOF if the EOF has been reached without being done
/// Returns -EDISK in case of disk error
static inline i8 jump_entries(const struct partition* part, struct file* dir,
    u32 entries) 
{
    return fat_inc_file_ptr(part, dir, entries * 32);
}


/// Takes in a dir pointer and increments the pointer so that it is pointing to 
/// the next valid entry in the directory
///
/// Returns  EEOF if entry is free and no subsequent entry are in use
/// Returns -EDISK if a disk error
/// Returns -EEOF in case of end of cluster chain
i8 get_next_valid_entry(const struct partition* part, struct file* dir)
{
    i8 err;

    // If the current one is a LFN jump past all LFN entries
    if (dir->cache[dir->page_offset + SFN_ATTR] == ATTR_LFN) {
        u8 cnt = (dir->cache[dir->page_offset] & LFN_SEQ_MSK);

        err = jump_entries(part, dir, cnt);
        if (err < 0)
            return err;
    }

    // The dir will point to the previous SFN entry. We have to jump untill we
    // find a used entry or the EOD
    do {
        err = jump_entries(part, dir, 1);
        if (err < 0)
            return err;

        u8 tmp = dir->cache[dir->page_offset];

        // Entry is free and no subsequent entries are in use
        if (tmp == 0x00)
            return EEOF;

        // The current entry is valid
        if (tmp != 0x05 && tmp != 0xE5)
            return 0;

    } while (1);
}


/// This takes in a file path and brakes it down into fragments. 
///
/// It takes in a full path starting with slash. It takes in the pointer to the
/// previous fragment (or / in case of the first one). This will return a pointer
/// to the next fragment if existing and return its size. It returns the length
/// of the found fragment, or 0 in case of no fragment
static u32 get_next_path_frag(const char* path, u32 size, const char** frag)
{
    const char* curr_frag = *frag;
    const char* end = path + size;

    // Iterate unil the start of the new fragment
    while ((curr_frag < end) && (*curr_frag != '/')) {
        curr_frag++;
    }

    // Skip the slash
    if (++curr_frag >= end) {
        return 0;
    }

    // Update the fragment - we will still return 0 if no fragment here
    *frag = curr_frag;

    // Iterate unil the start of the new fragment
    u32 len = 0;
    while ((curr_frag < end) && (*curr_frag != '/')) {
        curr_frag++;
        len++;
    }

    return len;
}


/// Sets the file pointer to point to the start of a new cluster. Returns a FAT
/// status code
static i8 set_cluster(const struct partition* part, struct file* file, u32 clust) 
{
    const struct fat* fat = part->fs;

    // The two first clusters are reserved
    if (clust < fat->root_clust_num) {
        return -EDISK;
    }

    file->page = ((clust - fat->root_clust_num) << fat->clust_order);
    file->page_offset = 0;

    return fat_cache(part, file);
}


/// Initialized a new file object. This can be avoided by allocating from 
/// kzmalloc
void file_struct_init(struct file* file)
{
    mem_set(file, 0x00, sizeof(struct file));
}


/// Points the direcory object to the root directory
static i8 dir_set_root(const struct partition* part, struct file* dir)
{
    dir->file_offset = 0;
    dir->page_offset = 0;
    dir->page = 0;

    return fat_cache(part, dir);
}

/// Returns if a dir object is the root directory
static u8 dir_is_root(const struct file* dir)
{
    return (dir->page == 0 && dir->page_offset == 0);
}


#define LFN_INDEX_SIZE 3

/// Index lookup for LFN name entries
struct {
    u8 size;
    u8 offset;
} const lfn_index[LFN_INDEX_SIZE] = {
    { .size = 5, .offset = 1 },
    { .size = 6, .offset = 14 },
    { .size = 2, .offset = 28 }
};


/// Remove this
/// REMOVE
static void print_entry(const struct partition* part, struct file* dir)
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
    return a;
}


/// Converts a char to lowercase
static inline char fat_to_lower(char a)
{
    if (a <= 'Z' && a >= 'A') {
        return a + 32;
    }
    return a;
}


/// Compare two chars case insensitive. Returns 1 if they are equal
static inline u8 fat_cmp_char_unified(char a, char b)
{
    a = fat_to_upper(a);
    b = fat_to_upper(b);

    return (u8)(a == b);
}


#define NUM_ILLEGAL_CHAR_SFN 16

/// List of all the illegal characters in the SFN file name
const u8 illegal_sfn_chars[NUM_ILLEGAL_CHAR_SFN] = {
    0x22, 0x2A, 0x2B, 0x2C, 0x2E, 0x2F, 0x3A, 0x3B,
    0x3C, 0x3D, 0x3E, 0x3F, 0x5B, 0x5C, 0x5D, 0x7C 
};


/// Returns if the given SFN character is a legal SFN character
static u8 is_sfn_char_valid(char sfn_char)
{
    // Check if the SFN char is valid
    for (u32 i = 0; i < NUM_ILLEGAL_CHAR_SFN; i++) {
        if (sfn_char == illegal_sfn_chars[i]) {
            return 0;
        }
    }

    // It cannot be lower than space
    if (sfn_char < 0x20) {
        return 0;
    }

    return 1;
}


/// Compares a SFN char agains the file name char and returns 1 if they match
static inline u8 fat_sfn_cmp_char(char sfn_char, char file_char)
{
    if (!is_sfn_char_valid(sfn_char)) {
        return 0;
    }

    // We do not compare if the SFN is space
    if (sfn_char == 0x20) {
        return 0;
    }

    // Convert the file char to uppercase. The spec requires all SFN chars to be
    // uppercase
    return (u8)(fat_to_upper(file_char) == sfn_char);
}


/// Converts a SFN 8.3 name to a normal filename. The buffer should at least be
/// 13 bytes long; 11 characters + 1 dot + NULL termination. This will
/// add the NULL terminating character at the end
static void fat_sfn_to_file_name(const char* sfn, char* buffer)
{
    u8 i;
    u8 j;

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
            // Extension is non-zero so we add a dot
            dot = 1;
        } else {
            // Extension done
            break;
        }

        // Fill in the extention
        buffer[i + j + 1] = fat_to_lower(tmp);
    }

    // Check if we have to update the dot
    if (dot) {
        buffer[i] = '.';
    }

    // NULL termination
    buffer[i + j + dot] = 0x00;
}


/// Converts a dot entry file name to a SFN 8.3 name
static u8 fat_dot_file_name_to_sfn(const char* file_name, u32 len, u8* sfn)
{
    // Either we have a . or a ..
    if (len > 2 && len == 0) {
        return 0;
    }

    // Fill the buffer with spaces
    for (u8 i = 0; i < 11; i++) {
        sfn[i] = ' ';
    }

    for (u8 i = 0; i < len; i++) {
        if (*file_name++ != '.') {
            return 0;
        }
        *sfn++ = '.';
    }
    return 1;
}


/// Converts a filename to a SFN 8.3 name
static u8 fat_file_name_to_sfn(const char* file_name, u32 len, u8* sfn)
{
    // Check if the file name is a dot entry name. If it is we have to handle
    // it differently
    if (*file_name == '.') {
        return fat_dot_file_name_to_sfn(file_name, len, sfn);
    }

    const char* file_tmp = file_name;
    u8 i;
    for (i = 0; (i < 8) && (i < len); i++) {

        // File name done
        if (*file_name == '.') {
            break;
        }

        // Check for a valid character
        if (!is_sfn_char_valid(*file_name)) {
            return 0;
        }

        // Spaces are not allowed in the SFN
        if (*file_name == 0x20) {
            return 0;
        }
       
        *sfn++ = fat_to_upper(*file_name++);
    }

    // Check for dot and padd with spaces
    if (*file_name != '.') {
        if (len >  i + 1) {
            return 0; 
        }
        for (; i < 11; i++) {
            *sfn++ = ' ';
        }
        return 1;
    }

    // We have a dot
    file_name++;

    // Padd the rest of the 8 file name
    for (; i < 8; i++) {
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
    for (; i < 11; i++) {
        *sfn++ = ' ';
    }
    return 1;
}


/// Compared a SFN entry 8.3 file name with a normal file name
static u8 fat_sfn_compare(const char* sfn, const char* file_name, u32 len)
{
    u8 sfn_buffer[11];

    // First convert the file name to SFN and then convert it
    if (!fat_file_name_to_sfn(file_name, len, sfn_buffer)) {
        return 0;
    }
    
    // Check if the match
    if (!mem_cmp(sfn_buffer, sfn, 11)) {
        return 0;
    }
    return 1;
}


/// Checks if the string `frag` with frag_length is present in the string
/// `file_name` at the specified offset
static u8 fat_lfn_cmp_frag(const char* file_name, u32 len, const char* frag,
    u32 frag_len, u32 offset)
{
    if (offset > len - 1) {
        return 0;
    }
    len -= offset;
    file_name += offset;

    u32 i;
    for (i = 0; (i < len) && (i < frag_len); i++) {
        print("%c", *frag);
        if (*file_name++ != *frag++) {
            print("\n");
            return 0;
        }
    }
    print("\n");

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


/// Convert a LFN dir entry to a 13-byte ASCII representation
static u8 fat_get_lfn_name(const char* lfn, char* lfn_buffer)
{
    u8 cnt = 0;
    for (u32 i = 0; i < LFN_INDEX_SIZE; i++) {
        for (u32 j = 0; j < lfn_index[i].size; ++j, ++cnt) {
            u8 offset = lfn_index[i].offset;
            u8 tmp = lfn[offset + j * 2];

            if (!tmp) {
                return cnt;
            }
            *lfn_buffer++ = tmp;
        }
    }
    return cnt;
}


/// Compares a directory entry agains a given file name. This will move the file
/// pointer so that it points to the SFN entry of the current block. 
///
/// Returns  ENOFILE if the filename does not match
/// Returns -EDISK if the search finds a disk error
/// Returns  0 if match
static i8 fat_compare_entry(const struct partition* part, struct file* dir, 
    const char* file_name, u32 len)
{
    i8 err;

    // Two cases - either the dir is pointing to a SFN or a chain of LFN entries
    // followed by a SFN
    const u8* entry = dir->cache + dir->page_offset;

    if (entry[SFN_ATTR] != ATTR_LFN) {
        // We are going to do one single SFN compare
        if (fat_sfn_compare((char *)entry, file_name, len) == 0) {
            return ENOFILE;
        }
        return 0;
    }

    // The dir should point to the first LFN entry. In this case bit 6 is set
    // in the LFN sequence number
    assert(entry[LFN_SEQ] & (1 << 6));

    // Compare the LFN file name
    u8 seq;
    u8 crc = entry[LFN_CRC];
    
    char lfn[13]; 
    do {
        // Update the sequence number
        seq = entry[LFN_SEQ] & LFN_SEQ_MSK;

        // Read the LFN name
        u8 lfn_size = fat_get_lfn_name((char *)entry, lfn);

        // Compare the LFN fragment and fix the dir pointer in case of a 
        // file mismatch
        if (!fat_lfn_cmp_frag(file_name, len, lfn, lfn_size, 13 * (seq - 1))) {
            
            err = jump_entries(part, dir, seq);

            // If this failes we have a disk error
            if (err < 0)
                return -EDISK;
            
            return ENOFILE;
        }

        // Move the directory entry window
        err = jump_entries(part, dir, 1);
        if (err < 0)
            return -EDISK;

        // Update the entry
        entry = dir->cache + dir->page_offset;

    } while (seq != 1);

    // We should at this point have dir pointing to the SFN following the last
    // LFN entry. The last thing to do is to check if the LFN chain is matching
    // the 8.3 SFN file name
    if (crc != fat_get_sfn_crc(entry)) {
        panic("CRC error");
        return -EDISK;
    }

    return 0;
}


/// Takes inn a dir pointing to the first LFN entry. It returns the LFN name
/// in `buffer` and returns the number of characters written in bytes.
///
/// Returns -EDISK in case of CRC mismatch or disk error
/// Returns -EEOF if the EOF has been reached without being done
static i8 get_lfn_full_name(const struct partition* part, struct file* dir, 
    char* buffer, u8* bytes_written)
{
    i8 err;
    u32 cnt = 0;
    u8* entry = dir->cache + dir->page_offset;

    // The dir should point to the first LFN entry. In this case bit 6 is set
    // in the LFN sequence number
    if ((entry[LFN_SEQ] & (1 << 6)) == 0) {
        return -EDISK;
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
        err = jump_entries(part, dir, 1);
        if (err < 0)
            return err;

        // Update the entry
        entry = dir->cache + dir->page_offset;

    } while (seq != 1);

    // We should at this point have dir pointing to the SFN following the last
    // LFN entry. The last thing to do is to check if the LFN chain is matching
    // the 8.3 SFN file name
    if (crc != fat_get_sfn_crc(entry)) {
        panic("CRC error");
        return -EDISK;
    }

    *bytes_written = cnt;
    return 0;
}


/// This function will search for a directory entry in the specified directory.
/// It will start the search from the current dir pointer.
///
/// Returns -EEOF in case of end of cluster chain
/// Returns -EDISK
/// Returns  EEOF if the directory is not found
static i8 fat_dir_search(const struct partition* part, struct file* dir,
    const char* file_name, u32 len)
{
    i8 err;
    print("Searching for: %*s\n", len, file_name);

    while (1) {
        //print_entry(part, dir);
        // Search after filename in the current dir entry
        err = fat_compare_entry(part, dir, file_name, len);
        if (err <= 0)
            return err;

        // Get the next entry
        err = get_next_valid_entry(part, dir);
        if (err < 0) 
            return err;
    }
}


/// This takes in a directory entry and return the cluster number which points
/// to either the file or the directory
static u32 fat_dir_ent_to_clust(const u8* dir_ent)
{
    u32 cluster = 0;

    cluster |= read_le16(dir_ent + SFN_CLUSTL);
    cluster |= read_le16(dir_ent + SFN_CLUSTH) << 16;

    return cluster;
}


/// Takes in a pointer to a directory and a relative path. It will return the 
/// file object to the resulting file or folder if existing. If the path is 
/// wrong it returns a path error.
///
/// Returns -EEOF in case of end of cluster chain
/// Returns -EDISK
/// Returns  EEOF if the directory is not found
static i8 fat_follow_path(const struct partition* part, struct file* dir,
    const char* path, u32 size)
{
    i8 err;
    const char* curr_frag = path;

    while (1) {
        u32 len = get_next_path_frag(path, size, &curr_frag);
        if (!len) {
            break;
        }

        // The file name is curr_frag with size len
        err = fat_dir_search(part, dir, curr_frag, len);
        if (err)
            return err;

        u8* ent_ptr = dir->cache + dir->page_offset;

        // We have a match in the path
        u32 clust = fat_dir_ent_to_clust(ent_ptr);
        dir->file_offset = 0;

        // If the directory is a .. and the cluster number is 0 go to root
        // TODO is this right?
        if (ent_ptr[0] == '.' && ent_ptr[1] == '.' && clust == 0) {
            err = dir_set_root(part, dir);
        } else {
            // Update the file size first
            dir->size = *(u32 *)(dir->cache + dir->page_offset + SFN_FILE_SIZE);
            err = set_cluster(part, dir, clust);
        }

        if (err < 0)
            return err;

        print("Going into directory %*s - cluster %d\n", len, curr_frag, clust);
    }

    return 0;
}


/// This function will take in a pointer to a directory. If this is a SFN it 
/// will remain unchanged. The buffer should be at least 256 bytes. 
///
/// If the dir point to a LFN entry is will parse the file name and move the dir
/// pointer to the SFN entry following that. If the user want to save the file
/// he can call the internal file save
///
/// Returns -EDISK in case of disk error
static i8 fat_get_entry_name(const struct partition* part, struct file* dir,
    char* buffer)
{
    // Check if the new entry is in use
    u8 tmp = dir->cache[dir->page_offset];

    if (tmp == 0x00) {
        return FAT_EOF;
    }
    if (tmp == 0x05 || tmp == 0xE5) {
        return FAT_ERROR;
    }

    // Check whether we have a LFN or a SFN
    if (dir->cache[dir->page_offset + SFN_ATTR] != ATTR_LFN) {
        fat_sfn_to_file_name((char *)(dir->cache + dir->page_offset), buffer);
        return 0;
    }

    // We have a LFN entry
    u8 cnt = 0;
    i8 err = get_lfn_full_name(part, dir, buffer, &cnt);
    if (err < 0)
        return err;

    // Fix the NULL character
    buffer[cnt] = 0;
    return 0;
}


/// Iternal structure for position saving when reading directory entries
struct file_ptr {
    u32 file_offset;
    u32 page_offset;
    u32 page;
};


/// Saves the internal position of a file into the lightweight file pointer
static void fat_file_save(const struct file* file, struct file_ptr* ptr)
{
    ptr->file_offset = file->file_offset;
    ptr->page_offset = file->page_offset;
    ptr->page = file->page;
}


/// Restores the internal position of a file from the lightweight file pointer
///
/// Returns -EDISK in case of disk error
static i8 fat_file_restore(const struct partition* part, struct file* file,
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


/// Public FAT32 file system API
/// For kernel internal use only
/// ----------------------------------------------------------------------------

/// Takes in a directory object and opens the directory pointed to by path. The
/// directory does not need to be closed after
i8 fat_dir_open(const struct partition* part, struct file* dir,
    const char* path, u32 size)
{
    i8 err;

    // Start at the root directory
    err = dir_set_root(part, dir);
    if (err < 0)
        return err;

    err = fat_follow_path(part, dir, path, size);
    if (err)
        return err;

    // Check if the opened file is acctually a directory
    if ((dir->cache[dir->page_offset + SFN_ATTR] & ATTR_DIR) == 0) {
        if (dir_is_root(dir) == 0) {
            return -ENODIR;
        }
    }
    return 0;
}

/// Takes in a directory pointer and reads the directory entry
///
/// Returns -EDISK in case of disk error
/// Returns -EEOF if the EOF has been reached without being done
i8 fat_dir_read(const struct partition* part, struct file* dir,
    struct file_info* info)
{
    i8 err;

    // Save the raw file pointer
    struct file_ptr ptr;
    fat_file_save(dir, &ptr);

    // Get the entry name and increase the dir pointer in case of LFN
    err = fat_get_entry_name(part, dir, info->name);
    if (err)
        return err;

    // Get the SFN info
    fat_get_sfn_info(dir->cache + dir->page_offset, info);

    // This only fails in case of disk error
    err = fat_file_restore(part, dir, &ptr);
    if (err)
        return err;

    return 0;
}

/// Gets the volume label
///
/// Returns -EDISK in case of disk error
/// Returns -EEOF if the EOF has been reached without being done
i8 fat_get_label(const struct partition* part, struct file_info* info)
{
    struct file* dir = kmalloc(sizeof(struct file));
    if (dir == NULL)
        return -ENOMEM;

    file_struct_init(dir);

    // Set the dir to the root directory
    i8 err = dir_set_root(part, dir);
    if (err)
        return err;

    while (1) {
        // Read one entry
        err = fat_dir_read(part, dir, info);
        if (err)
            break;

        // Check if it is the volume label
        if (info->attr & ATTR_VOL_LABEL) {
            break;
        }
    }
    kfree(dir);
    return err;
}

/// Read a number of bytes from a file and returns the numbr of of bytes
/// written
i8 fat_file_open(const struct partition* part, struct file* file,
    const char* path, u32 size)
{
    // Start at the root directory
    i8 err = dir_set_root(part, file);
    if (err)
        return err;

    err = fat_follow_path(part, file, path, size);
    if (err)
        return err;

    return 0;
}

i8 fat_file_read(const struct partition* part, struct file* file,
    u8* data, u32 req_cnt, u32* ret_cnt)
{
    // Check the parameters
    assert(file);
    assert(data);
    assert(ret_cnt);

    // Check if the file is zero
    if (file->size == 0) {
        *ret_cnt = 0;
        return -EEOF;
    }

    u32 i;
    i8 err = 0;
    for (i = 0; i < req_cnt; i++) {

        // Read the data into the buffer
        *data++ = file->cache[file->page_offset];

        err = fat_inc_file_ptr(part, file, 1);
        if (file->file_offset >= file->size) {
            i++;
            break;
        }

        // Break if error or end of file
        if (err) {
            i++;
            break;
        }
    }

    *ret_cnt = i;
    return err;
}

#define NW 20
#define DW 16
#define TW 10

void file_header(void)
{
    print("%-*s %-*s %-*s Size [bytes]\n", NW, "Name", DW, "Date modified", TW,
        "Type");
    print("-------------------------------------------------------------\n");
}

void file_print(struct file_info* info)
{
    // File name
    print("%-*s ", NW, info->name);

    // Date
    print("%02d/%02d/%04d %02d:%02d ", 
        info->write_date.day, info->write_date.month, info->write_date.year,
        info->write_time.hour, info->write_time.min);
    
    // Attributes
    if (info->attr & ATTR_DIR) {
        print("%-*s", TW, "folder");
    } else {
        print("%-*s", TW, "file");
    }

    // Print the size
    print(" %u", info->size);

    print("\n");
}

/// Called by the disk interface. This will take in a disk and try to mount it.
/// If it returns 1 the parition is fully mounted and can be accessed by the FAT
/// API. If it returns 0 it is probably not a FAT32 file system or it contains a
/// broken disk
i8 fat_mount_partition(struct partition* part)
{
    u8* buf = kmalloc(512);
    const struct disk* disk = part->parent_disk;

    // The the BPB block
    u32 status = disk->read(disk, part->start_lba, 1, buf);

    if (!status) {
        panic("Cant read FAT header");
    }

    // Check the FAT header signature is right
    if (!fat_signature_ok(buf)) {
        return -EDISK;
    }

    // Check if this is a FAT32 file system
    if (!bpb_contain_fat32(buf)) {
        return -EDISK;
    }

    // Allocate the FAT32 file system
    struct fat* fat = kmalloc(sizeof(struct fat));
    if (fat == NULL)
        return -ENOMEM;

    // Mount the file system
    build_fat_struct(part, fat, buf);

    // Free the BPB buffer
    kfree(buf);

    return 0;
}

#include <citrus/elf.h>

/// Test function for testing the file syatem
void fat_test(struct disk* disk)
{
    print("--- Running FAT test ---\n");
    return;
    
    // Open the root directory
    struct file* dir = dir_open("/sda2/wallpapers");
    if (!dir) 
    {
        print("Cannot open file\n");
        return;
    }

    struct file* file = file_open("/sda2/wallpapers/white-wallpaper.data", FILE_ATTR_R);
    if (file == NULL)
        print("Error\n");

    // Print the root directory
    struct file_info* info = kmalloc(sizeof(struct file_info));
    u8 status;
    file_header();
    while (1) {
        status = dir_read(dir, info);
        if (status != FAT_OK) {
            break;
        }
        file_print(info);
        get_next_valid_entry(dir->part, dir);
    };

    print("Done\n");
}
