/// Copyright (C) strawberryhacker

#ifndef FS_H
#define FS_H

#include <cinnamon/types.h>
#include <cinnamon/fat.h>

struct fs {
    void* priv;

};

struct file* opendir(const char* path);
u8 readdir(struct file* dir, struct file_info* info);
struct file* fopen(const char* path, u8 attr);

#endif
