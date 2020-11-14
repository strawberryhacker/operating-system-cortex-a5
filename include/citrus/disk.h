/// Copyright (C) strawberryhacker

#ifndef DISK_H
#define DISK_H

#include <citrus/types.h>
#include <citrus/list.h>

#define DISK_NAME_LEN 33

/// Holding the lists of system partitions and system volumes
struct sys_disk {
    struct list_node disks;
    struct list_node partitions;
};

/// Disk type
enum disk_type {
    DISK_SD,
    DISK_MMC
};

/// Used to identify the paritions in the MBR
struct part_table_entry {
    u8 status;
    u8 first_chs[3];
    u8 part_type;
    u8 last_chs[3];
    u32 lba;
    u32 sectors;
};

struct fat;

/// Each disk with a MBR format will contain four partitions. This will have a 
/// pointer (address) of the first sector in the partition and the partition 
/// size
struct partition {
    const struct fat* fs;
    const struct disk* disk;

    // Names for the partition
    char name[DISK_NAME_LEN];
    char label[256];
    u8 part_number;

    struct list_node node;

    // Describes the size of the volume
    u32 start_lba;
    u32 sect_count;
};

/// This describes a phyiscal disk. For example an SD card
struct disk {
    char name[DISK_NAME_LEN];
    enum disk_type type;

    void* priv;

    // List of all the disks in the system
    struct list_node node;

    struct partition partitions[4];

    u32 (*read)(const struct disk* disk, u32 sect, u32 cnt, u8* data);
};

void disk_init(void);
void disk_add(struct disk* disk, enum disk_type type);

// Prints all useable FAT32 file system partitions
void list_partitions(void);

struct partition* name_to_partition(const char* name, u8 cnt);

#endif
