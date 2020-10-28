/// Copyright (C) strawberryhacker

#include <cinnamon/fs.h>
#include <cinnamon/fat.h>
#include <cinnamon/kmalloc.h>
#include <cinnamon/disk.h>
#include <cinnamon/string.h>

/// Takes in a double pointer to a path and modifies it to point to the 
/// local path after stripping the partition name. This returns the partition
/// corresponding with the path or NULL
static struct partition* get_part_from_path(const char** path)
{
    const char* src = *path;
    // If the first char is a slash
    if (*src == '/') {
        src++;
    }

    // Get the partition name and length
    u8 cnt = 0;
    const char* frag = src;
    while ((*src) && (*src != '/') && (cnt < 0xFF)) {
         src++;
         cnt++;
    }

    *path = src;
    return name_to_partition(frag, cnt);
}

/// This functions opens a directory corresponding to the directory `path`. It
/// returns a file object pointing to the first entry in that directory
struct file* dir_open(const char* path)
{
    // Get the correct partition
    const struct partition* part = get_part_from_path(&path);
    if (!part) {
        return NULL;
    }

    // Allocate a new directory
    struct file* dir = kmalloc(sizeof(struct file));
    fat_file_init(dir);

    dir->part = part;

    // Try to open the relative path inside the partition
    u8 status = fat_dir_open(part, dir, path, string_length(path));
    if (status != FAT_OK) {
        return NULL;
    }
    return dir;
}

/// This functions reads the current directory pointed to by dir, and return
/// its file info
u8 dir_read(struct file* dir, struct file_info* info)
{
    if (!dir->part) {
        return FAT_ERROR;
    }
    return fat_dir_read(dir->part, dir, info);
}

struct file* fopen(const char* path, u8 attr);
