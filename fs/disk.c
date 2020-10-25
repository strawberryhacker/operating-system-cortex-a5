/// Copyright (C) strawberryhacker

#include <cinnamon/disk.h>
#include <cinnamon/print.h>
#include <cinnamon/string.h>
#include <cinnamon/mem.h>
#include <cinnamon/panic.h>
#include <cinnamon/kmalloc.h>
#include <cinnamon/fat.h>

static struct sys_disk sys_disk = {0};

void disk_init(void)
{
    list_init(&sys_disk.disks);
    list_init(&sys_disk.partitions);
}

/// Adds a disk to the system
static inline void add_disk(struct disk* disk)
{
    list_add_first(&disk->node, &sys_disk.disks);
}

/// Adds a partition to the system
static inline void add_partition(struct partition* vol)
{
    list_add_first(&vol->node, &sys_disk.partitions);
}

void list_disks(void)
{
    struct list_node* node;
    print("Disks \n");
    list_iterate(node, &sys_disk.disks) {
        struct disk* disk = list_get_entry(node, struct disk, node);
        print("%s\n", disk->name);
    }
}

void print_part(struct partition* part)
{
    struct disk* disk = part->parent_disk;
    print("%s%d\n", disk->name, part->part_number);
}

void list_partitions(void)
{
    struct list_node* node;
    print("Partitions \n");
    list_iterate(node, &sys_disk.partitions) {
        struct partition* part = list_get_entry(node, struct partition, node);
        print_part(part);
    }
}

/// Read the disk MBR (sector 0) and find the disk partitions and add them to 
/// the partitions
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

    struct part_table_entry* pte = (struct part_table_entry *)(buf + 446);

    for (u32 i = 0; i < 4; i++, pte++) {
        // Check if the partition is active
        if (pte->status >= 0x01 && pte->status <= 0x7F) {
            // Invalid
            continue;
        }

        // Check if this is a valid partition
        if (pte->sectors) {
            disk->partitions[i].sect_count = pte->sectors;
            disk->partitions[i].start_lba = pte->lba;

            disk->partitions[i].parent_disk = disk;
            disk->partitions[i].part_number = i;

            // This is a valid partition
            add_partition(&disk->partitions[i]);
        }
    }

    kfree(buf);
}

static char disk_get_letter(const char* name)
{
    char letter = 'a';
    struct list_node* it;
    list_iterate(it, &sys_disk.disks) {
        struct disk* disk = list_get_entry(it, disk, node);

        print("ok");
    }
}

void disk_add(struct disk* disk, const char* name)
{
    // Find a letter

    string_add_name(disk->name, name, DISK_NAME_LEN);
    add_disk(disk);

    list_disks();

    // Find all the partitions
    disk_find_partitions(disk);
    list_partitions();

    // Try to mount the disk
    for (u32 i = 0; i < 4; i++) {
        if (disk->partitions[i].sect_count) {

            // Try to mount the partition
            u32 status = fat_mount_partition(&disk->partitions[i]);
        }
    }

    fat_test(disk);
}

