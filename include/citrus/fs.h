/// Copyright (C) strawberryhacker

#ifndef FS_H
#define FS_H

#include <citrus/types.h>
#include <citrus/fat.h>

struct fs {
    void* priv;

};

#define FILE_ATTR_R  (1 << 0)
#define FILE_ATTR_RW (1 << 1)

struct file* file_open(const char* path, u8 attr);
struct file* dir_open(const char* path);

u8 file_close(struct file* file);
u8 dir_close(struct file* dir);

u8 dir_read(struct file* dir, struct file_info* info);
u8 file_read(struct file* file, u8* data, u32 req_cnt, u32* ret_cnt);

#endif
