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

static inline u32 check_fat_signature(const u8* bpb)
{
    if (bpb[510] == 0x55 && bpb[511] == 0xAA) {
        return 1;
    } else {
        return 0;
    }
}

/// Static fucntions
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

/// Read a little-endian half-word
u16 read_le16(const void* ptr)
{
    const u8* src = (const u8 *)ptr;
    
    u16 val = 0;
    
    val |= src[0] << 0;
    val |= src[1] << 8;

    return val;
}

/// Read a little-endian word
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
    /*
    for (u32 i = 0; i < 512;) {
        print("%02x", bpb[i]);
        if ((++i % 10) == 0) {
            print("\n");
        }
    }*/
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

    if (read_le16(bpb + BPB_ROOT_ENT_CNT) != 0) {
        // Not a FAT32 file system
        return 0;
    }

    // Determite the FAT system type by counting the number of cluster
    u32 fat_sectors;
    if (read_le16(bpb + BPB_FAT_SIZE_16)) {
        fat_sectors = read_le16(bpb + BPB_FAT_SIZE_16);
    } else {
        fat_sectors = read_le32(bpb + BPB_32_FAT_SIZE);
    }

    u32 tot_sectors;
    if (read_le16(bpb + BPB_TOT_SECT_16)) {
        tot_sectors = read_le16(bpb + BPB_TOT_SECT_16);
    } else {
        tot_sectors = read_le32(bpb + BPB_TOT_SECT_32);
    }

    u32 data_sectors = tot_sectors - read_le16(bpb + BPB_RSVD_CNT) - 
        (fat_sectors * bpb[BPB_NUM_FATS]);

    u32 data_clusters = data_sectors / bpb[BPB_CLUSTER_SIZE];

    if (data_clusters >= 65525) {
        return 1;
    } else {
        return 0;
    }
}

/// Mounts the FAT file system
static void fat_mount(struct fat* fat, u8* bpb)
{
    // Fat sizes
    fat->sect_size = read_le16(bpb + BPB_SECTOR_SIZE);
    fat->clust_size = bpb[BPB_CLUSTER_SIZE];
    fat->fats = bpb[BPB_NUM_FATS];

    // Get the offsets to the different parts
    fat->fat_off = read_le16(bpb + BPB_RSVD_CNT);
    fat->info_off = read_le16(bpb + BPB_32_FSINFO);

    fat->root_off = fat->fat_off + 
        (fat->fats * read_le32(bpb + BPB_32_FAT_SIZE));

    fat->root_clust = read_le32(bpb + BPB_32_ROOT_CLUST);
    
    // Add the short FAT name
    string_add_name(fat->label, (char *)bpb + BPB_32_VOL_LABEL, BPB_32_VOL_LABEL_SIZE);

    fat->fat_entries_per_sect = fat->sect_size / 4;
}

u32 fat_mount_partition(struct partition* part)
{
    // Read 512 byte
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

    // Allocate the FAT file system
    struct fat* fat = kmalloc(sizeof(struct fat));
    part->fs = fat;

    fat_mount(fat, buf);
    kfree(buf);

    print("FAT: %s\n", part->fs->label);

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
static inline u32 fat_file_to_lba(struct partition* part, struct file* file)
{
    struct fat* fat = part->fs;

    u32 lba = part->start_lba + fat->root_off + file->cluster_off +
         ((file->cluster - fat->root_clust) * fat->clust_size);

    return lba;
}

/// Follows the cluster chain of a file, incrementing the cluster number by 
/// count
static inline u8 fat_follow_cluster_chain(struct partition* part,
    struct file* file, u32* clust_ptr, u32 count)
{
    u32 clust = *clust_ptr;

    for (u32 i = 0; i < count; i++) {

        u32 lba = part->start_lba + part->fs->fat_off + 
            (clust / part->fs->fat_entries_per_sect);

        u32 off = clust % part->fs->fat_entries_per_sect;

        if (lba != file->fat_cache_lba) {
            // Cache another FAT sector
            struct disk* disk = part->parent_disk;
            u32 status = disk->read(disk, lba, 1, (u8 *)file->fat_cache);
            file->fat_cache_lba = lba;
        }

        // The FAT cache is holding the right sector
        u32 fat_entry = file->fat_cache[off];

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

/// Fetches an LBA sector into file cache
static u8 fat_cache(struct partition* part, struct file* file)
{
    /// Find the new LBA
    u32 lba = fat_file_to_lba(part, file);

    if (lba == file->cache_lba) {
        return FAT_OK;
    }

    // The LBA is not in the cache
    if (file->cache_dirty) {
        panic("Go implement file -> part -> disk -> mmc -> write");
        file->cache_dirty = 0;
    }

    // Fetch the new LBA into the cache
    struct disk* disk = part->parent_disk;
    
    u32 status = disk->read(disk, lba, 1, file->cache);

    if (!status) {
        return 0;
    }

    // Update the new LBA in cache
    file->cache_lba = lba;
    return 1;
}

/// Increments the rw offset by a number of bytes and caches any new sector
static u32 fat_inc_file_offset(struct partition* part, struct file* file,
    u32 inc)
{
    struct fat* fat = part->fs;

    file->file_offset += inc;
    file->sector_off += inc;

    // The sector is only 512 bytes and might get a sector overflow. This will
    // increase the sector number which might cause a cluster overflow. This 
    // will increment the cluster number which leads to a cluster chain walk.
    if (file->sector_off >= fat->sect_size) {
        file->cluster_off += file->sector_off / fat->sect_size;

        // Update the new sector offset
        file->sector_off = file->sector_off % fat->sect_size;

        // Check if this result in a cluster overflow
        if (file->cluster_off >= fat->clust_size) {
            u32 cluster_inc = file->cluster_off / fat->clust_size;
            print("Cluster walk: %d clusters\n", cluster_inc);

            // Update the new cluster offset
            file->cluster_off = file->cluster_off % fat->clust_size;

            u32 status = fat_follow_cluster_chain(part, file,
                &file->cluster, cluster_inc);         
        }

        // The cluster is resolved at this point. So chech if we have to cache
        // the new file sector
        fat_cache(part, file);
    }

    return 1;
}

static inline u32 fat_get_next_entry(struct partition* part, struct file* file) 
{
    return fat_inc_file_offset(part, file, 32);
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

/// Sets the files to the root directory
static u8 fat_file_set_root(struct partition* part, struct file* file)
{
    struct fat* fat = part->fs;

    file->cluster = fat->root_clust;
    file->sector_off = 0;
    file->cluster_off = 0;

    file->cache_lba = 0;

    u32 status = fat_cache(part, file);

    if (!status) {
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
static void print_entry(struct partition* part, struct file* dir, u32 entry)
{
    u8* entry_ptr = dir->cache + entry * 32;

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
}

static u32 fat_dir_search(struct partition* part, struct file* dir,
    const char* file_name, u32 len, struct file_info_short* info)
{
    print("YEY\n");

    for (u32 i = 0; i < 16; i++) {
        print_entry(part, dir, i);
    }

    return 1;
}

/// Takes in a pointer to a current directory, and it will follow the path given
/// by path and update the file (aka dir) object
static u32 fat_follow_path(struct partition* part, struct file* dir,
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
        struct file_info_short info;
        u32 status = fat_dir_search(part, dir, curr_frag, len, &info);

        if (!status) {
            return 0;
        }
    }

    return 1;
}

/// Public API
u32 fat_open_dir(struct partition* part, struct file* dir, const char* path, 
    u32 size)
{
    // In case clear the cache dirty
    dir->cache_dirty = 0;

    // Start at the root directory
    if (!fat_file_set_root(part, dir)) {

    }

    u32 status = fat_follow_path(part, dir, path, size);

    return 1;
}

u32 fat_dir_read(struct partition* part, struct file* file,
    struct file_info* info)
{
    return 1;
}

void fat_test(struct disk* disk)
{
    print("FAT test functions\n");

    struct partition* part = &disk->partitions[1];
    u8* buf = kmalloc(512);

    print("Root offset => %d\n", part->fs->root_off);
    print("LBA => %d\n", part->start_lba);

    disk->read(disk, part->start_lba + part->fs->root_off, 1, buf);
    struct file* file = kmalloc(sizeof(struct file));

    char str[] = "/dev/tty/yo.txtasdkahsdkjahskajh/asdkjas/asdkjh/yoo";
    u32 status = fat_open_dir(part, file, str, sizeof(str) - 1);
}
