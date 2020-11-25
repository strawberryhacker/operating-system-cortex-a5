// Copyright (C) strawberryhacker

#include <citrus/elf.h>
#include <citrus/print.h>
#include <citrus/thread.h>
#include <citrus/mm.h>
#include <citrus/atomic.h>
#include <citrus/page_alloc.h>
#include <citrus/mem.h>
#include <citrus/sched.h>
#include <citrus/pt_entry.h>
#include <citrus/cache.h>
#include <citrus/align.h>
#include <citrus/cache.h>
#include <citrus/panic.h>
    #include <stddef.h>

// ELF type
enum elf_type {
    ET_NONE,
    ET_REL,
    ET_EXEC,
    ET_DYN,
    ET_CORE
};

static const char* elf_type_str[] = {
    "ET_NONE",
    "ET_REL",
    "ET_EXEC",
    "ET_DYN",
    "ET_CORE"
};

// Section type
enum sect_type {
    SHT_NULL,
    SHT_PROGBITS,
    SHT_SYMTAB,
    SHT_STRTAB,
    SHT_RELA,
    SHT_HASH,
    SHT_DYNAMIC,
    SHT_NOTE,
    SHT_NOBITS,
    SHT_REL,
    SHT_SHLIB,
    SHT_SYNSYM
};

static const char* sect_type_str[] = {
    "SHT_NULL",
    "SHT_PROGBITS",
    "SHT_SYMTAB",
    "SHT_STRTAB",
    "SHT_RELA",
    "SHT_HASH",
    "SHT_DYNAMIC",
    "SHT_NOTE",
    "SHT_NOBITS",
    "SHT_REL",
    "SHT_SHLIKB",
    "SHT_DYNSYM"
};

// Segment types
enum prog_type {
    PT_NULL,
    PT_LOAD,
    PT_DYNAMIC,
    PT_INTERP,
    PT_NOTE,
    PT_SHLIB,
    PT_PHDR
};

static const char* prog_type_str[] = {
    "PT_NULL",
    "PT_LOAD",
    "PT_DYNAMIC",
    "PT_INTERP",
    "PT_NOTE",
    "PT_SHLIB",
    "PT_PHDR"
};

static void print_mem(const u8* addr, u32 size, u32 col);

// Takes in an ELF file and returns the number of bytes for the binary. This 
// will ignore any NOLOAD sections in the ELF file
u32 elf_get_binary_size(const u8* data, u32 size)
{
    struct elf_header* header = (struct elf_header *)data;
    if (size < header->phoff + header->phnum * header->phentsize) {
        print("Size error\n");
        return 0;
    }

    // Get the start of the program header section table
    struct elf_prog_header* prog_header = 
        (struct elf_prog_header* )(data + header->phoff);
    
    u32 bin_size = 0;
    for (u32 i = 0; i < header->phnum; i++) {

        // Check if the program section is loadable
        if (prog_header->type == PT_LOAD) {
            bin_size += prog_header->filesz;
        }
    }

    return bin_size;
}

// This does OBJCOPY on the given elf file and produces a runnable binary. The
// binary has to be able to contain the entrie binary. elf_get_binary_size 
// should be called first
void objcopy(const u8* elf, u32 elf_size, u8* binary)
{
    u8* bin_ptr = binary;
    struct elf_header* header = (struct elf_header *)elf;

    // Get the start of the program header section table
    struct elf_prog_header* prog_header = 
        (struct elf_prog_header* )(elf + header->phoff);
    
    u32 size = 0;
    for (u32 i = 0; i < header->phnum; i++) {

        // Check if the program section is loadable
        if (prog_header->type == PT_LOAD) {
            u32 tmp_size = prog_header->filesz;

            const u8* src = elf + prog_header->offset;
            mem_copy(src, binary, tmp_size);

            binary += tmp_size;
            size += tmp_size;
        }
    }

    // Just print the binary
    print_mem(bin_ptr, size, 16);
}

static void print_mem(const u8* addr, u32 size, u32 col)
{
    u32 i = 1;
    for (u32 j = 0; j < size; j++) {
        print("%02X ", *addr++);
        if ((i++ % col) == 0) {
            print("\n");
        }
    }
}

void print_sect_header(struct elf_sect_header* ptr, char* str_table)
{
    print("Section: %s\n", str_table + ptr->name);
    print("Name: %08X\n", ptr->name);
    if (ptr->type <= 11) {
        print("Type: %s\n", sect_type_str[ptr->type]);
    } else {
        print("Unknown\n");
    }
    
    print("Flags: %08X\n", ptr->flags);
    print("Addr: %08X\n", ptr->addr);
    print("Offset: %08X\n", ptr->offset);
    print("Size: %08X\n", ptr->size);
    print("Link: %08X\n", ptr->link);
    print("Info: %08X\n", ptr->info);
    print("Addralign: %08X\n", ptr->addralign);
    print("Entsize: %08X\n\n", ptr->entsize);   
}

void print_prog_header(struct elf_prog_header* ptr)
{
    if (ptr->type <= 11) {
        print("Type: %s\n", prog_type_str[ptr->type]);
    } else {
        print("Unknown\n");
    }
    
    print("Offset: %08X\n", ptr->offset);
    print("Vaddr: %08X\n", ptr->vaddr);
    print("Paddr: %08X\n", ptr->paddr);
    print("Filesz: %08X\n", ptr->filesz);
    print("Memsz: %08X\n", ptr->memsz);
    print("Flags: %08X\n", ptr->flags);
    print("Align: %08X\n", ptr->align);
}

void elf_init(const u8* elf_data, u32 elf_size)
{
    struct elf_header* elf_header = (struct elf_header *)elf_data;

    u32 elf_type = elf_header->type;
    if (elf_header->version != 1) {
        print("Error\n");
    }
    /*
    print("Entry addr: %08X\n", elf_header->entry);
    print("Phoff: %08X\n", elf_header->phoff);
    print("Shoff: %08X\n", elf_header->shoff);
    print("Flags: %08X\n", elf_header->flags);
    print("Ehsize: %04X\n", elf_header->ehsize);
    print("Program header size: %04X\n", elf_header->phentsize);
    print("Program header number: %04X\n", elf_header->phnum);
    print("Section header size: %04X\n", elf_header->shentsize);
    print("Section header number: %04X\n", elf_header->shnum);
    print("Section header string index addr: %04X\n", elf_header->shstrndx);
    */

    struct elf_prog_header* prog_header = 
        (struct elf_prog_header *)(elf_data + elf_header->phoff);

    // Allocate and copy the loadable data
    u32 bin_pages = (u32)align_up_ptr((void *)prog_header->memsz, 4096) / 4096;
    u32 bin_order = pages_to_order(bin_pages);
    struct page* bin_page_ptr = alloc_pages(bin_order);
    
    mem_copy(elf_data + prog_header->offset, page_to_va(bin_page_ptr),
        prog_header->filesz);


    // Create a process
    u32 irq = __atomic_enter();
    struct thread* t = create_process((i32 (*)(void *))elf_header->entry, 500,
        "elf-app", NULL, SCHED_RT);

    struct pte_attr attr = {
        .access = PTE_ACCESS_FULL_ACC,
        .mem    = PTE_MEM_WRITE_THROUGH,
        .domain = 15,
        .nG     = 0,
        .xn     = 0
    };

    i32 err = mm_map_in_pages(t->mmap, bin_page_ptr, (1 << bin_order),
        prog_header->vaddr, &attr);

    assert(err == 1);

    asm volatile("dsb" : : :"memory");
    asm volatile("dmb" : : :"memory");
    asm volatile("isb" : : :"memory");
    dcache_clean();
    icache_invalidate();

    __atomic_leave(irq);

}
