// Copyright (C) strawberryhacker

#include <citrus/disk.h>
#include <citrus/print.h>
#include <citrus/string.h>
#include <citrus/mem.h>
#include <citrus/panic.h>
#include <citrus/kmalloc.h>
#include <citrus/error.h>
#include <citrus/fat.h>

static const char* disk_names[] = {
    "sd",
    "mmc"
};

struct sys_disk sys_disk = {0};

void disk_init(void)
{
    list_init(&sys_disk.disks);
    list_init(&sys_disk.partitions);
}

// Adds a disk to the system
static inline void add_disk(struct disk* disk)
{
    list_add_last(&disk->node, &sys_disk.disks);
}

// Adds a partition to the system
static inline void add_partition(struct partition* vol)
{
    list_add_last(&vol->node, &sys_disk.partitions);
}

// Print a list of all the disks in the system
void list_disks(void)
{
    struct list_node* node;
    print("Disks \n");
    list_iterate(node, &sys_disk.disks) {
        struct disk* disk = list_get_entry(node, struct disk, node);
        print("> %s\n", disk->name);
    }
}

// Print a list of all the partitions in the system
void list_partitions(void)
{
    struct list_node* node;
    print("Partitions \n");
    list_iterate(node, &sys_disk.partitions) {
        struct partition* part = list_get_entry(node, struct partition, node);
        print("> %s %s\n", part->name, part->label);
    }
}

// Sets the name on a partition based on the disk and the partition number
static void partition_set_name(const struct disk* disk, struct partition* part,
    u8 index)
{
    const char* src = disk->name;
    char* dest = part->name;

    // Set the first part of the name
    while (*src) {
        *dest++ = *src++;
    }

    // Set the parition number
    if (index > 9) {
        panic("Unsupported feature!");
    }

    *dest++ = '1' + index;
    *dest = 0x00;
}

// Read the disk MBR (sector 0) and find the disk partitions and add them to 
// the partitions. This will give all valid partitins a name
void disk_find_partitions(struct disk* disk)
{
    mem_set(disk->partitions, 0, sizeof(disk->partitions));

    // Read the MBR at sector zero
    u8* buf = kmalloc(512);
    
    if (!disk->read(disk, 0, 1, buf)) {
        panic("Cant read MBR");
    }
    
    // Check is this is a valid MBR
    if (buf[510] != 0x55 || buf[511] != 0xAA) {
        return;
    }

    // Partition table entry
    struct part_table_entry* pte = (struct part_table_entry *)(buf + 446);

    u8 part_index = 0;
    for (u32 i = 0; i < 4; i++, pte++) {
        // Check if the partition is active
        if (pte->status >= 0x01 && pte->status <= 0x7F) {
            // Invalid
            continue;
        }

        // Check if this is a valid partition
        if (read_le32(&pte->sectors)) {

            struct partition* part = &disk->partitions[i];
            part->sect_count = read_le32(&pte->sectors);
            part->start_lba = read_le32(&pte->lba);

            part->disk = disk;
            part->part_number = i;

            // Set the partition name
            partition_set_name(disk, part, part_index);
            part_index++;
        }
    }
    kfree(buf);
}

// This function will take in a disk and a disk type and give the disk a name
void disk_set_name(struct disk* disk, enum disk_type type)
{
    // The name is determined from the disk type
    char letter = 'a';

    struct list_node* node;
    list_iterate(node, &sys_disk.disks) {
        struct disk* disk = list_get_entry(node, struct disk, node);
        if (disk->type == type) {
            letter++;
        }
    }
    
    // Copy the name
    const char* src = disk_names[type];
    char* dest = disk->name;
    while (*src) {
        *dest++ = *src++;
    }

    // Set the letter and NULL termination
    *dest++ = letter;
    *dest = 0x00;
}

// Compares a partition string and a name
static u8 partition_name_cmp(const char* part_name, const char* name, u8 cnt)
{
    cnt++;
    while (--cnt && *part_name) {
        if (*part_name++ != *name++) {
            return 0;
        }
    }
    return ((cnt == 0) && (*part_name == 0));
}

// Returns the partition of the given name
struct partition* name_to_partition(const char* name, u8 cnt)
{
    struct list_node* node;

    list_iterate(node, &sys_disk.partitions) {
        struct partition* part = list_get_entry(node, struct partition, node);
        if (partition_name_cmp(part->name, name, cnt)) {
            return part;
        }
    }
    return NULL;
}

// Adds a new disk to the system
void disk_add(struct disk* disk, enum disk_type type)
{
    // Find a letter
    disk_set_name(disk, type);

    // Add the disk to the system
    add_disk(disk);

    // Find all the partitions on the MBR
    disk_find_partitions(disk);

    // Allocate onf file info for the file name
    struct file_info* info = kmalloc(sizeof(struct file_info));

    // Try to mount the partitions
    for (u32 i = 0; i < 4; i++) {
        struct partition* part = &disk->partitions[i];
        if (part->sect_count) {

            // Try to mount the partition
            i32 err = fat_mount_partition(part);
            if (err == -ENOMEM)
                panic("No memory");

            if(err)
                continue;

            // Add the partition
            add_partition(part);
            string_copy("No label", part->label);
        }
    }
    list_partitions();
}

