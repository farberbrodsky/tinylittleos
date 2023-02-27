#include <kernel/util.hpp>
#include <kernel/scheduler/elf.hpp>
#include <kernel/logging.hpp>

typedef uint16_t Elf_Half;
typedef uint32_t Elf_Off;
typedef uint32_t Elf_Addr;
typedef uint32_t Elf_Word;
typedef int32_t  Elf_Sword;

static constexpr inline size_t ELF_NIDENT = 16;

struct __attribute__((packed)) Elf32_Phdr {
	Elf_Word p_type;
	Elf_Off  p_offset;
	Elf_Addr p_vaddr;
	Elf_Addr p_paddr;
	Elf_Word p_filesz;
	Elf_Word p_memsz;
	Elf_Word p_flags;
	Elf_Word p_align;
};

static constexpr Elf_Word PT_LOAD = 1;
static constexpr Elf_Word PF_W = 2;

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
        return (e_ident[0] = 0x7F && e_ident[1] == 'E' && e_ident[2] == 'L' && e_ident[3] == 'F');
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

errno elf_loader::load_elf(fs::file_desc *file, void *&entry) {
    Elf_Ehdr hdr;

    // read whole header
    uint64_t pos = 0;
    ssize_t read_size;
    while (pos < sizeof(hdr)) {
        read_size = file->pread(reinterpret_cast<char *>(&hdr) + pos, sizeof(hdr) - pos, pos);
        if (read_size < 0)       return static_cast<errno>(read_size);
        else if (read_size == 0) return errno::invalid;
        pos += read_size;
    }

    entry = reinterpret_cast<void *>(hdr.e_entry);

    if (!hdr.check_supported()) return errno::invalid;
    kassert(hdr.e_phentsize == sizeof(Elf32_Phdr));
    kassert(hdr.e_phnum < 1000);

    // supported header: go to program header table
    for (Elf_Half i = 0; i < hdr.e_phnum; i++) {
        // read program header
        Elf32_Phdr phdr;

        pos = hdr.e_phoff + i * sizeof(Elf32_Phdr);
        size_t phdr_pos = 0;
        while (phdr_pos < sizeof(phdr)) {
            read_size = file->pread(reinterpret_cast<char *>(&phdr) + phdr_pos, sizeof(phdr) - phdr_pos, pos + phdr_pos);
            if (read_size < 0)       return static_cast<errno>(read_size);
            else if (read_size == 0) return errno::invalid;
            phdr_pos += read_size;
            // TODO returning an error without freeing anything...
        }

        TINY_WARN("p_flags ", phdr.p_flags, " p_offset ", phdr.p_offset, " p_memsz ", phdr.p_memsz, " p_filesz ", phdr.p_filesz, " p_vaddr ", formatting::hex{phdr.p_vaddr}, " p_type ", phdr.p_type);

        // For now, stupid implementation: just allocate all of the type=LOAD pages in hmem, no idea how to free yet
        // Will need a rb tree and virtual memory in future

        if (phdr.p_type == PT_LOAD) {
            // allocate pages for MemSiz aligned to 4096
            reg_t addr_start = phdr.p_vaddr & 0xFFFFF000u;
            reg_t addr_end   = addr_start + phdr.p_memsz;
            kassert((addr_start & 0xFFF) == 0);

            for (char *virt = reinterpret_cast<char *>(addr_start);
                 reinterpret_cast<reg_t>(virt) < addr_end;
                 virt += 0x1000) {

                TINY_INFO("map at ", formatting::hex{virt});
                // map page at [virt, virt + 4096)
                // TODO make sure it's not overriding another page
                memory::phys_t new_page = memory::hmem_alloc_page();
                memory::map_user_page(virt, new_page, (phdr.p_flags & PF_W) != 0);
                reg_t idx = reinterpret_cast<reg_t>(virt) - addr_start;
                reg_t memset_at = 0;

                if (idx < phdr.p_filesz) {
                    // read from file
                    reg_t read_cnt = phdr.p_filesz - idx;
                    if (read_cnt > 0x1000) read_cnt = 0x1000;
                    else                   memset_at = read_cnt;
                    idx += phdr.p_offset;

                    pos = 0;
                    while (pos < read_cnt) {
                        read_size = file->pread(virt + pos, read_cnt - pos, idx + pos);
                        if (read_size < 0)       return static_cast<errno>(read_size);
                        else if (read_size == 0) return errno::invalid;
                        // TODO returning an error without freeing anything...
                        pos += read_size;
                    }
                }

                memset(virt + memset_at, 0, 0x1000 - memset_at);
            }
        }
    }

    return errno::ok;
}
