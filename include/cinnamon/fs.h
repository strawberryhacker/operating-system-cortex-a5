/// Copyright (C) strawberryhacker

#ifndef FS_H
#define FS_H

#include <cinnamon/types.h>
#include <cinnamon/fat.h>

struct fs {
    void* priv;

};

struct file* file_open(const char* path, u8 attr);
struct file* dir_open(const char* path);

u8 file_close(struct file* file);
u8 dir_close(struct file* dir);

u8 dir_read(struct file* dir, struct file_info* info);

#endif
