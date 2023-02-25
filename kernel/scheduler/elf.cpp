#include <kernel/util.hpp>

typedef uint16_t Elf_Half;
typedef uint32_t Elf_Off;
typedef uint32_t Elf_Addr;
typedef uint32_t Elf_Word;
typedef int32_t  Elf_Sword;

static constexpr inline size_t ELF_NIDENT = 16;

struct __attribute__((packed)) Elf_Ehdr {
    // \0x7F E L F Architecture ByteOrder ELFVersion OSSpecific OSSpecific Padding
    uint8_t   e_ident[ELF_NIDENT];
    Elf_Half  e_type;
    Elf_Half  e_machine;
    Elf_Word  e_version;
    Elf_Addr  e_entry;
    Elf_Off   e_phoff;      // program header table position
    Elf_Off   e_shoff;
    Elf_Word  e_flags;
    Elf_Half  e_ehsize;
    Elf_Half  e_phentsize;  // program header table entry size
    Elf_Half  e_phnum;      // program header table entry count
    Elf_Half  e_shentsize;
    Elf_Half  e_shnum;
    Elf_Half  e_shstrndx;

    inline bool check_magic() {
        return e_ident[0] = 0x7F && e_ident[1] == 'E' && e_ident[2] == 'L' && e_ident[3] == 'F';
    }

    inline bool check_supported() {
        return
            check_magic()   &&
            e_ident[4] == 1 &&  // architecture
            e_ident[5] == 1 &&  // little endian
            e_machine == 3  &&  // architecture x86
            e_ident[6] == 1 &&  // version
            e_type == 2;        // executable file, not relocatable
    }
};

struct __attribute__((packed)) Elf_ProgramHeader {
    Elf_Word  p_type;   // 0 = ignore, 1 = load: p_memsz bytes are 0 then copy p_filesz bytes from p_offset to p_vaddr, 2 = requires dynamic linking, 3 = interp, 4 = note
    Elf_Off   p_offset;
    Elf_Addr  p_vaddr;
    Elf_Word  reserved;
    Elf_Word  p_filesz;
    Elf_Word  p_memsz;
    Elf_Word  p_flags;  // 1 = exec, 2 = writable, 4 = readable
    Elf_Word  p_alignment;
};
