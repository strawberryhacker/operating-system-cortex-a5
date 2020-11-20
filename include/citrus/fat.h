/// Copyright (C) strawberryhacker

#ifndef FAT_H
#define FAT_H

#include <citrus/types.h>
#include <citrus/disk.h>

/// Old BPB and BS
#define BPB_JUMP_BOOT		0
#define BPB_OEM				3
#define BPB_SECTOR_SIZE		11
#define BPB_CLUSTER_SIZE	13
#define BPB_RSVD_CNT		14
#define BPB_NUM_FATS		16
#define BPB_ROOT_ENT_CNT	17
#define BPB_TOT_SECT_16		19
#define BPB_MEDIA			21
#define BPB_FAT_SIZE_16		22
#define BPB_SEC_PER_TRACK	24
#define BPB_NUM_HEADS		26
#define BPB_HIDD_SECT		28
#define BPB_TOT_SECT_32		32

/// New BPB and BS applying for FAT12 and FAT16
#define BPB_16_DRV_NUM		36
#define BPB_16_RSVD1		37
#define BPB_16_BOOT_SIG		38
#define BPB_16_VOL_ID		39
#define BPB_16_VOL_LABEL	43
#define BPB_16_FSTYPE		54

// New BPB and BS applying for FAT32
#define BPB_32_FAT_SIZE		  36
#define BPB_32_EXT_FLAGS	  40
#define BPB_32_FSV			  42
#define BPB_32_ROOT_CLUST	  44
#define BPB_32_FSINFO		  48
#define BPB_32_BOOT_SECT	  50
#define BPB_32_RSVD			  52
#define BPB_32_DRV_NUM		  64
#define BPB_32_RSVD1		  65
#define BPB_32_BOOT_SIG		  66
#define BPB_32_VOL_ID	      67
#define BPB_32_VOL_LABEL      71
#define BPB_32_VOL_LABEL_SIZE 12
#define BPB_32_FSTYPE         82

/// Directory entry defines
#define SFN_NAME			0
#define SFN_ATTR			11
#define SFN_NTR				12
#define SFN_CTIME_TH		13
#define SFN_CTIME			14
#define SFN_CDATE			16
#define SFN_ADATE			18
#define SFN_CLUSTH			20
#define SFN_WTIME			22
#define SFN_WDATE			24
#define SFN_CLUSTL			26
#define SFN_FILE_SIZE		28

/// Long file name defines
#define LFN_SEQ				0
#define LFN_SEQ_MSK			0x1F
#define LFN_NAME_1			1
#define LFN_ATTR			11
#define LFN_TYPE			12
#define LFN_CRC				13
#define LFN_NAME_2			14
#define LFN_NAME_3			28

/// File attribute defines
#define ATTR_RO				0x01
#define ATTR_HIDD			0x02
#define ATTR_SYS			0x04
#define ATTR_VOL_LABEL		0x08
#define ATTR_DIR			0x10
#define ATTR_ARCH			0x20
#define ATTR_LFN			0x0F

/// FSinfo structure
#define INFO_CLUST_CNT		488
#define INFO_NEXT_FREE		492

/// FAT table defines
#define FAT_ENTRY_MASK 0xFFFFFFF

/// Status defines in the file system
#define FAT_OK         0x00
#define FAT_DISK_ERROR 0x01
#define FAT_ERROR      0x02
#define FAT_BAD_CLUST  0x04
#define FAT_BAD_PATH   0x08 
#define FAT_EOF        0x10
#define FAT_EOCC       0x20

/// Error mask for hard fauts. The rest of the faults does not nessecarily 
/// terminate the function
#define FAT_ERROR_MASK (FAT_DISK_ERROR | FAT_BAD_CLUST | FAT_ERROR)

/// Main FAT file system structure. Every disk (partition) with a valid FAT32
/// will have a pointer to this structure. There is one structure per file 
/// system
struct fat {
    // We use the order and the mask for fast division and modulo
    u32 page_order;
    u32 page_mask;
    u32 clust_order;
    u32 clust_mask;
    u32 fat_ent_order;
    u32 fat_ent_mask;

    // Holds the virtual index of the root cluster
    u32 root_clust_num;

    // Global page numbers
    u32 data_start;
    u32 fat_start;
    u32 bpb_start;
    u32 info_start;

    // Number of FAT tables
    u8 fats;
};

/// Structure describing a file and a directory
struct file {
    // Holds the offset whithin a file
    u32 file_offset;
    u32 offset;
    u32 page;

    u32 size;

    // Working buffer
    u8 cache[512];
    u32 cache_page;
    u8 cache_dirty;

    // FAT cache for caching 128 FAT entries
    u32 fat_cache[128];
    u32 fat_cache_page;
    u8 fat_cache_dirty;

    // Buffer for LFN calculation
    u8 lfn_buffer[256];
    u32 lfn_offset;

    // Add a pointer to the partition
    const struct partition* part;

    // Misc
    struct list_node open;
    struct list_node parent;
};

struct file_date {
    u16 year;
    u8 month;
    u8 day;
};

struct file_time {
    u16 hour : 5;
    u16 min  : 6;
    u16 sec  : 5;
};

struct file_info {
    // File name for the user either a SFN or a LFN
    // terminated with a NULL
    char name[256];

    u8 attr;

    // File time
    struct file_time create_time;
    struct file_date create_date;
    struct file_time write_time;
    struct file_date write_date;
    struct file_date access_date;

    // Size of the file or directory
    u32 size;

};

// REMOVE
void fat_test(struct disk* disk);

// REMOVE
i32 fat_mount_partition(struct partition* part);

void file_struct_init(struct file* file);

// NOTE that the following functions take in the path relative to the partition.
// These paths therefore start with `/dir/file.txt` relative to root
i32 fat_dir_open(struct file* dir, const char* path, u32 size);
i32 fat_dir_read(struct file* dir, struct file_info* info);
i32 fat_file_open(struct file* file, const char* path, u32 size);
i32 fat_file_read(struct file* file, u8* data, u32 req_cnt, u32* ret_cnt);
i32 get_next_valid_entry(struct file* dir);

// REMOVE
void file_print(struct file_info* info);
void file_header(void);

#endif
