/// Copyright (C) strawberryhacker

#ifndef ELF_H
#define ELF_H

#include <citrus/types.h>

#define MAGIC_CHARS 16

/// Main ELF header
struct elf_header {
    u8 magic[MAGIC_CHARS];
    u16 type;
    u16 machine;
    u32 version;
    u32 entry;
    u32 phoff;
    u32 shoff;
    u32 flags;
    u16 ehsize;
    u16 phentsize;
    u16 phnum;
    u16 shentsize;
    u16 shnum;
    u16 shstrndx;
};

/// Section header
struct elf_sect_header {
    u32 name;
    u32 type;
    u32 flags;
    u32 addr;
    u32 offset;
    u32 size;
    u32 link;
    u32 info;
    u32 addralign;
    u32 entsize;
};

/// Symbol table entry
struct elf_sym_table {
    u32 name;
    u32 value;
    u32 size;
    u8 info;
    u8 other;
    u16 shndx;
};

/// Program header
struct elf_prog_header {
    u32 type;
    u32 offset;
    u32 vaddr;
    u32 paddr;
    u32 filesz;
    u32 memsz;
    u32 flags;
    u32 align;
};

void elf_init(const u8* elf_data, u32 elf_size);

#endif
