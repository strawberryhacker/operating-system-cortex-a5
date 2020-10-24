/// Copyright (C) strawberryhacker

#ifndef FAT_H
#define FAT_H

#include <cinnamon/types.h>
#include <cinnamon/disk.h>

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

/// Status defines in the file system
#define FAT_EOCC 1
#define FAT_OK   0

struct fat {
    char label[BPB_32_VOL_LABEL_SIZE];

    // Private file system info
    u32 sect_size;
    u32 clust_size;
    u32 fat_entries_per_sect;
    u8 fats;

    u32 fat_off;
    u32 root_off;
    u32 root_clust;
    u32 info_off;
};

/// Structure describing a file and a directory
struct file {
    u32 file_offset;
    u32 cluster;
    u32 cluster_off;
    u32 sector_off;

    // Working buffer
    u8 cache[512];
    u32 cache_lba;
    u8 cache_dirty;

    // FAT cache for caching 128 FAT entries
    u32 fat_cache[128];
    u32 fat_cache_lba;

    // Buffer for LFN calculation
    u8 lfn_buffer[256];
    u32 lfn_offset;

    // Add a pointer to the partition
    struct partition* part;
};

struct file_info {
    
};

void fat_test(struct disk* disk);

u32 fat_mount_partition(struct partition* part);

u32 fat_open_dir(struct partition* part, struct file* dir, const char* path, 
    u32 size);

u32 fat_dir_read(struct partition* part, struct file* dir,
    struct file_info* info);


#endif
