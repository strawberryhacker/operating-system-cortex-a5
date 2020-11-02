/// Copyright (C) strawberryhacker

#include <citrus/fat.h>
#include <citrus/kmalloc.h>
#include <citrus/panic.h>
#include <citrus/mem.h>
#include <citrus/string.h>
#include <citrus/disk.h>
#include <citrus/fs.h>
#include <citrus/page_alloc.h>

/// This file implements the Microsoft® FAT32 file system. The implementation
/// supports the LFN extension as well as the known 8.3 file name. 
///
/// Each function in this API takes in a partition. This should have a pointer
/// to its parent disk whitch again should provide the disk read / write API. If
/// the disk or parition in not correcly configured this interface might fail.
/// The used should provide a custom inteface which implements the functions
/// using absolute path. This interface should parse the first part of the file
/// path to obtain the disk and partition and call this API with the target 
/// partition and the file path with the disk spcified stripped
///
/// This file is for internal use by the operating system

/// Prints the FAT status code to the serial console
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
static inline u8 check_fat_signature(const u8* bpb)
{
    if (bpb[510] == 0x55 && bpb[511] == 0xAA) {
        return 1;
    } else {
        return 0;
    }
}

/// Dumps a memory region to the seiral console
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

/// Returns a little-endian half-word pointed to by ptr
static u16 read_le16(const void* ptr)
{
    const u8* src = (const u8 *)ptr;
    u16 val = 0;
    
    val |= src[0] << 0;
    val |= src[1] << 8;

    return val;
}

/// Read a little-endian word pointed to by ptr
static u32 read_le32(const void* ptr)
{
    const u8* src = (const u8 *)ptr;
    u32 val = 0;

    val |= src[0] << 0;
    val |= src[1] << 8;
    val |= src[2] << 16;
    val |= src[3] << 24;

    return val;
}

/// Checks if the BPB given hold a valid FAT file system. Returns 1 if the BPB
/// belongs to any FAT 12/16/32 file system
static u8 is_fat(const u8* bpb)
{
    if (mem_cmp(bpb + BPB_16_FSTYPE, "FAT", 3)) {
        return 1;
    }
    if (mem_cmp(bpb + BPB_32_FSTYPE, "FAT", 3)) {
        return 1;
    }
    return 0;
}

/// Checks if the BPB belongs to a FAT32 files system. Returns 1 if it is a
/// valid FAT32 file system
static u8 bpb_is_fat32(const u8* bpb)
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
    
    return (data_pages >= 65525);
}

/// This converts a order to a bitmask. For example 3 would return 0b111. The
/// functions returns a 32 bit mask
static u32 fat_order_to_mask(u32 order)
{
    u32 res = 0;
    for (u32 i = 0; i < order; i++) {
        res |= (1 << i);
    }
    return res;
}

/// Mounts the FAT file system by fillng in the FAT structure elements. This
/// assumes that the file system is allready a FAT32 file system. It will not do
/// any file system checks on the BPB. 
static void fat_force_mount(struct partition* part, struct fat* fat, 
    u8* bpb)
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
    
    // Add the short FAT file system label
    string_add_name(fat->label, (char *)bpb + BPB_32_VOL_LABEL,
        BPB_32_VOL_LABEL_SIZE);

    // Mount it
    part->fs = fat;
}

/// Returns 1 if the directory entry is a LFN entry
static u8 fat_is_lfn(const u8* dir_entry)
{
    u32 attr = dir_entry[LFN_ATTR];
    if (attr == ATTR_LFN) {
        return 1;
    }
    return 0;
}

/// The file pointer has a local page offset from whitin the data section start
/// in the current parition. This functions will compute the global page address
/// of this file. This can be used directly with the disk API to read and write
/// pages
static inline u32 fat_file_to_page(const struct partition* part, 
    const struct file* file)
{
    return file->page + part->fs->root_glob_page;
}

/// Caches the FAT table page pointed to by glob_page. Returns a FAT status
static u8 fat_cache_fat_page(const struct partition* part, struct file* file,
    u32 glob_page)
{
    if (glob_page != file->fat_cache_glob_page) {
        
        // The global page is currently not in the cache so we have to cache it
        const struct disk* disk = part->parent_disk;
        u32 status = disk->read(disk, glob_page, 1, (u8 *)file->fat_cache);
        if (!status) {
            return FAT_DISK_ERROR;
        }

        // A new page has been cached 
        file->fat_cache_glob_page = glob_page;
    }
    return FAT_OK;
}

/// Returns the FAT status based on a cluster
static u8 fat_get_fat_ent_status(u32 fat_entry)
{
    // If not done; mask the entry
    fat_entry &= FAT_ENTRY_MASK;

    // Check for a normal cluster
    if (fat_entry >= 0x0000002 && fat_entry <= 0xFFFFFEF) {
        return FAT_OK;
    }

    // Check if the entry is valid. The cluster can either be bad, free,
    // allocated or EOCC (end of cluster chain)
    if (fat_entry >= 0xFFFFFF8 && fat_entry <= 0xFFFFFFF) {
        return FAT_EOCC | FAT_OK;
    }
    
    // The code should not get here. This indicates an hard error on disk.
    // For debugging we panic here
    panic("Cluster error!");

    // Bad page in cluster
    if (fat_entry == 0xFFFFFF7) {
        return FAT_BAD_CLUST;
    }

    // Any reserved clusters
    if (fat_entry < 0x0000002 || fat_entry > 0xFFFFFEF) {
        return FAT_BAD_CLUST; 
    }

    return FAT_OK;
}

/// This will increment the clust_ptr by `count` clusters and update the
/// destination cluster when returning. This is NOT done by adding count to the
/// cluster pointer. Returns a FAT status indicating either error, success or
/// success plus EOCC
static u8 fat_follow_cluster_chain(const struct partition* part,
    struct file* file, u32* clust_ptr, u32 count)
{
    const struct fat* fat = part->fs;

    // Initial cluster value
    u32 clust = *clust_ptr;

    for (u32 i = 0; i < count; i++) {

        // We need to find the global page and the FAT entry offset whithin a
        // FAT table page
        u32 page = fat->fat_glob_page + (clust >> fat->fat_ent_order);
        u32 entry = clust & fat->fat_ent_mask;

        // Try cache the global FAT table page in the FAT cache
        if (fat_cache_fat_page(part, file, page) != FAT_OK) {
            return FAT_DISK_ERROR;
        }

        // The FAT cache is holding the right page
        u32 fat_entry = file->fat_cache[entry] & FAT_ENTRY_MASK;
        u8 status = fat_get_fat_ent_status(fat_entry);

        // Cluster is the last on or have errors
        if (status != FAT_OK) {
            return status;
        }

        clust = fat_entry;
    }

    // Write the new cluster back to the cluster pointer. This will probably be
    // an entry in the file structure
    *clust_ptr = clust;
    return FAT_OK;
}

/// Caches the current file pointer page into the file cache. Returning a
/// FAT status code
static u8 fat_cache(const struct partition* part, struct file* file)
{
    /// Find the new page address
    u32 page = fat_file_to_page(part, file);
    if (page == file->cache_glob_page) {
        return FAT_OK;
    }

    // The LBA is not in the cache
    if (file->cache_dirty) {
        panic("Disk write implementation missing!");
        file->cache_dirty = 0;
    }

    // Fetch the new page into the cache
    const struct disk* disk = part->parent_disk;
    
    u32 status = disk->read(disk, page, 1, file->cache);
    if (!status) {
        return FAT_DISK_ERROR;
    }

    // Update the new LBA in cache
    file->cache_glob_page = page;
    return FAT_OK;
}

/// Increments the file pointer by a number of bytes and caches the new page. 
/// Returns a FAT status code
static inline u8 fat_inc_file_ptr(const struct partition* part,
    struct file* file, u32 bytes)
{
    const struct fat* fat = part->fs;

    file->file_offset += bytes;
    file->page_offset += bytes;

    // Check if we have to resolve the page offset in case of byte overflow
    if (file->page_offset & ~fat->page_mask) {

        // Find how many pages we have to increment
        u32 page_inc = file->page_offset >> fat->page_order;
        file->page_offset &= fat->page_mask;

        // The page increment might overflow the current cluster
        u32 curr_cluster = (file->page >> fat->clust_order) +
            fat->root_clust_num;
        u32 next_cluster = ((file->page + page_inc) >> fat->clust_order) + 
            fat->root_clust_num;

        // Find how many clusters we have to jump
        u32 clust_jump = next_cluster - curr_cluster;

        // If cluster jump is non-zero we have to follow the cluster chain in
        // the file allocation table
        if (clust_jump) {
            u8 status = fat_follow_cluster_chain(part, file, &curr_cluster,
                clust_jump);

            // The cluster chain walk might fail because of FAT cache update
            if (status & (FAT_ERROR_MASK | FAT_EOCC)) {
                return status;
            }

            // If we have a cluster overflow, the page number might have changed
            file->page = ((file->page + page_inc) & fat->clust_mask) + 
                ((curr_cluster - fat->root_clust_num) << fat->clust_order);

        } else {
            file->page += page_inc;
        }

        // If the page number has changed we have to cache another buffer
        u8 status = fat_cache(part, file);
        if (status & FAT_ERROR_MASK) {
            return status;
        }
    }
    return FAT_OK;
}

/// Jumps `entries` number of entries in the directory. This will increment the
/// file pointer by 32 bytes
static inline u8 fat_jump_entries(const struct partition* part,
    struct file* dir, u32 entries) 
{
    return fat_inc_file_ptr(part, dir, entries * 32);
}

/// Takes in a dir pointer and increment the pointer so that it is pointing to 
/// the next valid entry in the directory. This is returns a FAT status code. 
/// It might return some kind of FAT_ERROR, FAT_EOD or FAT_OK
static u8 fat_get_next_entry(const struct partition* part, struct file* dir)
{
    // If the current one is a LFN jump past all LFN entries
    if (dir->cache[dir->page_offset + SFN_ATTR] == ATTR_LFN) {
        u8 cnt = (dir->cache[dir->page_offset] & LFN_SEQ_MSK);
        u8 status = fat_jump_entries(part, dir, cnt);
        if (status & FAT_ERROR_MASK) {
            return status;
        }
    }

    // The dir will point to the previous SFN entry. We have to jump untill we
    // find a used entry or the EOD
    while (1) {
        u8 status = fat_jump_entries(part, dir, 1);
        if (status & FAT_ERROR_MASK) {
            return status;
        }
        u8 tmp = dir->cache[dir->page_offset];

        // Check for end EOD marker
        if (tmp == 0x00) {

            // Entry is free and no subsequent entries are in use
            return FAT_EOF;
        }
        if (tmp != 0x05 && tmp != 0xE5) {
            // The current entry is valid
            return FAT_OK;
        }
    } 
}

/// This takes in a file path and brakes it down into fragments. 
///
/// It takes in a full path starting with slash. It takes in the pointer to the
/// previous fragment (or / in case of the first one). This will return a pointer
/// to the next fragment if existing and return its size. It returns the length
/// of the found fragment, or 0 in case of no fragment
static u32 fat_get_next_path_frag(const char* path, u32 size, const char** frag)
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

    // Update the fragment - will still will return 0 if no fragment here
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
static u8 fat_set_cluster(const struct partition* part, struct file* file,
    u32 clust) 
{
    const struct fat* fat = part->fs;

    // The two first clusters are reserved
    if (clust < fat->root_clust_num) {
        return FAT_BAD_CLUST;
    }

    file->page = ((clust - fat->root_clust_num) << fat->clust_order);
    file->page_offset = 0;

    return fat_cache(part, file);
}

/// Initialized a new file object
void fat_file_init(struct file* file)
{
    // Clear caches
    file->cache_dirty = 0;
    file->cache_glob_page = 0;
    file->fat_cache_dirty = 0;
    file->fat_cache_glob_page = 0;

    // Clear offsets
    file->file_offset = 0;
    file->page_offset = 0;
    file->page = 0;
}

/// Points the direcory object to the root directory
static u8 fat_file_set_root(const struct partition* part, struct file* dir)
{
    dir->file_offset = 0;
    dir->page_offset = 0;
    dir->page = 0;

    u32 status = fat_cache(part, dir);
    if (status & FAT_ERROR_MASK) {
        return status;
    }
    return FAT_OK;
}

/// Returns if a dir object is the root directory
static u8 dir_is_root(const struct file* dir)
{
    if (dir->page == 0 && dir->page_offset == 0) {
        return 1;
    }
    return 0;
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

/// Remove this
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
static u8 fat_sfn_char_valid(char sfn_char)
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
    if (!fat_sfn_char_valid(sfn_char)) {
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
        if (!fat_sfn_char_valid(*file_name)) {
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

/// Compared a SFN entry 8.3 file name with a normal file name. Returns 1 if 
/// ther is a perfect match
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

/// Converts the USC LFN characters to ASCII and returns them in a 13 bytes
/// LFN buffer. Returns how many bytes were acctually written
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
/// pointer so that it points to the SFN entry of the current block. Returns 
/// a FAT status code
static u8 fat_compare_entry(const struct partition* part, struct file* dir, 
    const char* file_name, u32 len)
{
    // Two cases - either the dir is pointing to a SFN or a chain of LFN entries
    // followed by a SFN
    const u8* entry = dir->cache + dir->page_offset;

    if (entry[SFN_ATTR] != ATTR_LFN) {
        // We are going to do one single SFN compare
        if (fat_sfn_compare((char *)entry, file_name, len)) {
            return FAT_OK;
        } else {
            return FAT_BAD_PATH;
        }
    }

    // The dir should point to the first LFN entry. In this case bit 6 is set
    // in the LFN sequence number
    assert(entry[LFN_SEQ] & (1 << 6));

    // Compare the LFN file name
    u8 seq;
    u8 crc = entry[LFN_CRC];
    
    char lfn[13]; 
    do {
        // Update the sequnce number
        seq = entry[LFN_SEQ] & LFN_SEQ_MSK;

        // Read the LFN name
        u8 lfn_size = fat_get_lfn_name((char *)entry, lfn);

        // Compare the LFN fragment and fix the dir pointer in case of a 
        // file mismatch
        if (!fat_lfn_cmp_frag(file_name, len, lfn, lfn_size, 13 * (seq - 1))) {
            
            u8 status = fat_jump_entries(part, dir, seq);
            if (status & FAT_ERROR_MASK) {
                return status;
            }
            return FAT_BAD_PATH;
        }

        // Move the directory entry window
        u8 status = fat_jump_entries(part, dir, 1);
        if (status & FAT_ERROR_MASK) {
            return status;
        }

        // Update the entry
        entry = dir->cache + dir->page_offset;

    } while (seq != 1);

    // We should at this point have dir pointing to the SFN following the last
    // LFN entry. The last thing to do is to check if the LFN chain is matching
    // the 8.3 SFN file name
    if (crc != fat_get_sfn_crc(entry)) {
        panic("CRC error");
        return FAT_DISK_ERROR;
    }

    return FAT_OK;
}

/// Takes inn a dir pointing to the first LFN entry. It returns the LFN name
/// in `buffer` and returns the number of characters written in bytes. This
/// returns a FAT status code
static u8 fat_get_lfn_full_name(const struct partition* part, struct file* dir, 
    char* buffer, u8* bytes)
{
    u32 cnt = 0;
    u8* entry = dir->cache + dir->page_offset;

    // The dir should point to the first LFN entry. In this case bit 6 is set
    // in the LFN sequence number
    if ((entry[LFN_SEQ] & (1 << 6)) == 0) {
        return FAT_ERROR;
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
        u8 status = fat_jump_entries(part, dir, 1);
        if (status & FAT_ERROR_MASK) {
            return status;
        }

        // Update the entry
        entry = dir->cache + dir->page_offset;

    } while (seq != 1);

    // We should at this point have dir pointing to the SFN following the last
    // LFN entry. The last thing to do is to check if the LFN chain is matching
    // the 8.3 SFN file name
    if (crc != fat_get_sfn_crc(entry)) {
        panic("CRC error");
        return FAT_ERROR;
    }
    *bytes = cnt;
    return FAT_OK;
}

/// This will take in a directory and search for the file_name in that 
/// directory. The file pointer will point to either the EOF or the SFN entry
/// of the matching file or folder. Returns a FAT status code
static u8 fat_dir_search(const struct partition* part, struct file* dir,
    const char* file_name, u32 len)
{
    print("Searching for: %*s\n", len, file_name);

    while (1) {
        //print_entry(part, dir);
        // Search after filename in the current dir entry
        u8 status = fat_compare_entry(part, dir, file_name, len);
        if (status & FAT_ERROR_MASK) {
            return status;
        }

        // Match?
        if (status == FAT_OK) {
            print(RED "Match\n" NORMAL);
            return FAT_OK;
        }

        // Get the next entry
        status = fat_get_next_entry(part, dir);
        if (status != FAT_OK) {
            return status;
        }
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
static u8 fat_follow_path(const struct partition* part, struct file* dir,
    const char* path, u32 size)
{
    const char* curr_frag = path;

    while (1) {
        u32 len = fat_get_next_path_frag(path, size, &curr_frag);
        if (!len) {
            break;
        }

        // This has a directory pointed to by file and a file name pointed to 
        // by curr_frag and len
        u32 status = fat_dir_search(part, dir, curr_frag, len);
        if (status != FAT_OK) {
            return status;
        }
        u8* ent_ptr = dir->cache + dir->page_offset;

        // We have a match in the path
        u32 clust = fat_dir_ent_to_clust(ent_ptr);
        dir->file_offset = 0;

        // If the directory is a .. and the cluster number is 0 go to root
        if (ent_ptr[0] == '.' && ent_ptr[1] == '.' && clust == 0) {
            status = fat_file_set_root(part, dir);
        } else {
            // Update the file size first
            dir->size = *(u32 *)(dir->cache + dir->page_offset + SFN_FILE_SIZE);
            status = fat_set_cluster(part, dir, clust);
        }

        if (status != FAT_OK) {
            fat_status(status);
            return status;
        }

        print("Going into directory %*s - cluster %d\n", len, curr_frag, clust);
    }

    return FAT_OK;
}

/// This function will take in a pointer to a directory. If this is a SFN it 
/// will remain unchanged. The buffer should be at least 256 bytes. 
///
/// If the dir point to a LFN entry is will parse the file name and move the dir
/// pointer to the SFN entry following that. If the user want to save the file
/// he can call the internal file save. Returns a FAT status code
static u8 fat_get_entry_name(const struct partition* part, struct file* dir,
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

        // SFN
        fat_sfn_to_file_name((char *)(dir->cache + dir->page_offset), buffer);

        return FAT_OK;
    }

    // We have a LFN entry
    u8 cnt = 0;
    u8 status = fat_get_lfn_full_name(part, dir, buffer, &cnt);
    if (status & FAT_ERROR_MASK) {
        return status;
    }

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
static void fat_file_save(const struct file* file, struct file_ptr* ptr)
{
    ptr->file_offset = file->file_offset;
    ptr->page_offset = file->page_offset;
    ptr->page = file->page;
}

/// Restores the internal position of a file from the lightweight file pointer
static u8 fat_file_restore(const struct partition* part, struct file* file,
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
/// For internal kernel use only
/// ----------------------------------------------------------------------------

/// Takes in a directory object and opens the directory pointed to by path. The
/// directory does not need to be closed after
u8 fat_dir_open(const struct partition* part, struct file* dir,
    const char* path, u32 size)
{
    // Start at the root directory
    u8 status = fat_file_set_root(part, dir);
    if (status != FAT_OK) {
        return status;
    }
    status = fat_follow_path(part, dir, path, size);

    // Check for errors
    if (status != FAT_OK) {
        return status;
    }

    // Check if the opened file is acctually a directory
    if ((dir->cache[dir->page_offset + SFN_ATTR] & ATTR_DIR) == 0) {
        if (dir_is_root(dir) == 0) {
            return FAT_BAD_PATH;
        }
    }
    return FAT_OK;
}

/// Takes in a directory pointer and reads the directory entry
u8 fat_dir_read(const struct partition* part, struct file* dir,
    struct file_info* info)
{
    // Save the raw file pointer
    struct file_ptr ptr;
    fat_file_save(dir, &ptr);

    // Get the entry name and increase the dir pointer in case of LFN
    u8 status = fat_get_entry_name(part, dir, info->name);
    if (status != FAT_OK) {
        return status;
    }

    // Get the SFN info
    fat_get_sfn_info(dir->cache + dir->page_offset, info);

    // This only fails in case of disk error
    status = fat_file_restore(part, dir, &ptr);
    if (status != FAT_OK) {
        return status;
    }

    return FAT_OK;
}

/// Gets the volume label
u8 fat_get_label(const struct partition* part, struct file_info* info)
{
    struct file* dir = kmalloc(sizeof(struct file));
    fat_file_init(dir);

    // Set the dir to the root directory
    u8 status = fat_file_set_root(part, dir);
    if (status & FAT_ERROR_MASK) {
        return status;
    }

    while (1) {
        // Read one entry
        status = fat_dir_read(part, dir, info);
        if (status & (FAT_ERROR_MASK | FAT_EOF)) {
            kfree(dir);
            return status;
        }

        // Check if it is the volume label
        if (info->attr & ATTR_VOL_LABEL) {
            kfree(dir);
            return FAT_OK;
        }
    }
}

/// Read a number of bytes from a file and returns the numbr of of bytes
/// written
u8 fat_file_open(const struct partition* part, struct file* file,
    const char* path, u32 size)
{
    // Start at the root directory
    u8 status = fat_file_set_root(part, file);
    if (status != FAT_OK) {
        return status;
    }
    status = fat_follow_path(part, file, path, size);

    // Check for errors
    if (status != FAT_OK) {
        return status;
    }

    // Check if the opened file is acctually a directory
    if (file->cache[file->page_offset + SFN_ATTR] & ATTR_DIR) {
        if (dir_is_root(file) == 0) {
            return FAT_BAD_PATH;
        }
    }
    return FAT_OK;
}

u8 fat_file_read(const struct partition* part, struct file* file,
    u8* data, u32 req_cnt, u32* ret_cnt)
{
    // Check the parameters
    assert(file);
    assert(data);
    assert(ret_cnt);

    // Check if the file is zero
    if (file->size == 0) {
        *ret_cnt = 0;
        return FAT_EOF;
    }

    u32 i;
    u8 status = FAT_OK;
    for (i = 0; i < req_cnt; i++) {

        // Read the data into the buffer
        *data++ = file->cache[file->page_offset];

        status = fat_inc_file_ptr(part, file, 1);
        if (file->file_offset >= file->size) {
            i++;
            break;
        }

        // Break if error or end of file
        if (status & (FAT_ERROR_MASK | FAT_EOCC)) {
            i++;
            break;
        }
    }

    *ret_cnt = i;
    return status;
}

#define NW 20
#define DW 16
#define TW 10

static void file_header(void)
{
    print("%-*s %-*s %-*s Size [bytes]\n", NW, "Name", DW, "Date modified", TW,
        "Type");
    print("-------------------------------------------------------------\n");
}

static void file_print(struct file_info* info)
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
u32 fat_mount_partition(struct partition* part)
{
    u8* buf = kmalloc(512);
    const struct disk* disk = part->parent_disk;

    // The the BPB block
    u32 status = disk->read(disk, part->start_lba, 1, buf);

    if (!status) {
        panic("Cant read FAT header");
    }

    // Check the FAT header signature is right
    if (!check_fat_signature(buf)) {
        return 0;
    }

    // Check if this is a FAT32 file system
    if (!bpb_is_fat32(buf)) {
        return 0;
    }

    // Allocate the FAT32 file system
    struct fat* fat = kmalloc(sizeof(struct fat));

    // Mount the file system
    fat_force_mount(part, fat, buf);

    // Free the BPB buffer
    kfree(buf);

    return 1;
}

/// Test function for testing the file syatem
void fat_test(struct disk* disk)
{
    print("--- Running FAT test ---\n");

    // Open the root directory
    struct file* dir = dir_open("/sda2/application/");
    if (!dir) 
    {
        print("Cannot open file\n");
        return;
    }

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
        fat_get_next_entry(dir->part, dir);
    };

    // Test file read
    struct file* elf = 
        file_open("/sda2/application/build/application.elf", FILE_ATTR_R);

    if (elf == NULL) {
        return;
    }

    print("ELF size => %d\n", elf->size);

    u32 ret_cnt;

    // Getting the allocation size in `order`
    u32 elf_order = bytes_to_order(elf->size);

    // Allocating the pages
    struct page* elf_alloc = alloc_pages(elf_order);
    u8* elf_buffer = page_to_va(elf_alloc);
    u8* buffer = elf_buffer;
    do {
        u8 status = file_read(elf, buffer, 512000, &ret_cnt);
        print("Bytes acctually written => %d\n", ret_cnt);
        if (status != FAT_OK) {
            fat_status(status);
            break;
        }
        buffer += 512;
    } while (ret_cnt == 512);

    print("Done\n");
}
