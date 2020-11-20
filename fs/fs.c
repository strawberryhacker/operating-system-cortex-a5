// Copyright (C) strawberryhacker

#include <citrus/fs.h>
#include <citrus/fat.h>
#include <citrus/kmalloc.h>
#include <citrus/disk.h>
#include <citrus/string.h>
#include <citrus/panic.h>
#include <citrus/error.h>

// Takes in a double pointer to a path and modifies it to point to the 
// local path after stripping the partition name. This returns the partition
// corresponding with the path or NULL
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

// This functions opens a directory corresponding to the directory `path`. It
// returns a file object pointing to the first entry in that directory
struct file* dir_open(const char* path)
{
    // Get the correct partition
    const struct partition* part = get_part_from_path(&path);
    if (!part)
        return NULL;

    // Allocate a new directory
    struct file* dir = kzmalloc(sizeof(struct file));
    dir->part = part;

    // Try to open the relative path inside the partition
    i32 err = fat_dir_open(dir, path, string_length(path));
    if (err) {
        kfree(dir);
        return NULL;
    }
    return dir;
}

// This functions reads the current directory pointed to by dir, and return
// its file info
i32 dir_read(struct file* dir, struct file_info* info)
{
    assert(dir->part);
    return fat_dir_read(dir, info);
}

// Opens a file. If takes in a global path and returns a file object
struct file* file_open(const char* path, u8 attr)
{
    // Get the partition from the file name
    const struct partition* part = get_part_from_path(&path);
    if (part == NULL)
        return NULL;

    // Allocate a new file
    struct file* file = kzmalloc(sizeof(struct file));
    file->part = part;

    // Try to open the file
    i32 err = fat_file_open(file, path, string_length(path));
    if (err) {
        kfree(file);
        return NULL;
    }
    
    return file;
}

// This will read a number of bytes from a file pointed to by `file`. It will
// return the acctual number of bytes written
i32 file_read(struct file* file, u8* data, u32 req_cnt, u32* ret_cnt)
{
    assert(file->part);

    // Read from a file
    return fat_file_read(file, data, req_cnt, ret_cnt);
}

