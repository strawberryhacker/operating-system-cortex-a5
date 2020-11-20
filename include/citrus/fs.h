/// Copyright (C) strawberryhacker

#ifndef FS_H
#define FS_H

#include <citrus/types.h>
#include <citrus/fat.h>

struct fs {
    void* priv;

};

#define FILE_R  (1 << 0)
#define FILE_RW (1 << 1)

struct file* file_open(const char* path, u8 attr);
struct file* dir_open(const char* path);

i32 file_close(struct file* file);
i32 dir_close(struct file* dir);

i32 dir_read(struct file* dir, struct file_info* info);
i32 file_read(struct file* file, u8* data, u32 req_cnt, u32* ret_cnt);

#endif
