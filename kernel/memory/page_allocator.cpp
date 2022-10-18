#include <kernel/memory/page_allocator.hpp>
#include <kernel/memory/multiboot.hpp>
#include <kernel/util.hpp>
#include <kernel/util/string.hpp>
#include <kernel/util/asm_wrap.hpp>
#include <kernel/logging.hpp>
#include <kernel/tty.hpp>
using namespace memory;

extern "C" {
extern char _kernel_start;
extern char _kernel_end;
}

// current implementation of page table allocation:
// - kmem : Identity-mapped memory which is at 0xC0000000 and up, for most kernel objects.
//          kmem is always supervisor, writable pages, so it can be recycled too.
// - hmem : High memory, which can't be identity mapped due to address space limitations.
//          For example userspace process memory and cache are in hmem.
//          We can individually map high memory pages when needed at the top of the address space.
//
// When kmem and hmem collide we crash...

// allocator globals

static phys_t kmem_phys_end;  // highest physical address used by kmem + 1 (e.g. 0x1000 instead of 0xFFF which is end of page)
static phys_t hmem_phys_end;  // lowest  physical address used by hmem
static uint32_t hmem_end;     // lowest  virtual  address used to map hmem
// highest virtual address used by kmem is kmem_phys_end.to_virt()

__attribute__((aligned(4096)))
static uint32_t first_page_directory[1024];

static inline uint32_t encode_pte(uint32_t addr) {
    return addr;
}
template <class... Flags>
static inline uint32_t encode_pte(uint32_t addr, page_flag flag, Flags... flags) {
    return encode_pte(addr | static_cast<uint32_t>(flag), flags...);
}

// used to allocate memory when we need a new page table
static phys_t free_page_table;

void *memory::kmem_alloc_page() {
    scoped_intlock lock;  // block interrupts

    phys_t new_page_phys {kmem_phys_end};
    kmem_phys_end = phys_t(kmem_phys_end.value() + 4096);

    uint32_t virt = new_page_phys.to_virt();
    uint32_t dir_index = (uint32_t)virt >> 22;
    uint32_t tab_index = (uint32_t)virt >> 12 & 0x03FF;

    uint32_t &dir_entry = first_page_directory[dir_index];
    uint32_t *page_table;

    if (dir_entry & (uint32_t)page_flag::present) {
        page_table = (uint32_t *)(void *)(phys_t(dir_entry).align_page_down().to_virt());
        page_table[tab_index] = encode_pte(new_page_phys.value(), page_flag::present, page_flag::write);
    } else {
        // need to allocate new page table, use the available free page table
        page_table = (uint32_t *)(void *)(free_page_table.to_virt());
        memset(page_table, 0, 4096);
        dir_entry = encode_pte(free_page_table.value(), page_flag::present, page_flag::write);
        page_table[tab_index] = encode_pte(new_page_phys.value(), page_flag::present, page_flag::write);
        // now that there is another page table - next allocation should have a page table
        free_page_table = phys_t::from_kmem(kmem_alloc_page());
    }

    return (void *)virt;
}

void memory::kmem_free_page(void *addr) {
    scoped_intlock lock;  // block interrupts
    kunused(addr);
    // TODO TODO TODO implement...
}

static void map_page(void *virt, uint32_t pde_flags, uint32_t pte) {
    scoped_intlock lock;  // block interrupts
    kassert(((uint32_t)virt & 0xFFF) == 0);  // make sure address is page aligned

    uint32_t dir_index = (uint32_t)virt >> 22;
    uint32_t tab_index = (uint32_t)virt >> 12 & 0x03FF;

    uint32_t &dir_entry = first_page_directory[dir_index];
    uint32_t *page_table;

    if (dir_entry & (uint32_t)page_flag::present) {
        // page table already exists!
        page_table = (uint32_t *)(void *)(phys_t(dir_entry).align_page_down().to_virt());
        page_table[tab_index] = pte;
    } else {
        // need to allocate new page table
        page_table = (uint32_t *)kmem_alloc_page();
        if (dir_entry & (uint32_t)page_flag::present) [[unlikely]] {
            // kmem_alloc_page created a page table for us, so we don't need this one anymore
            page_table = (uint32_t *)(void *)(phys_t(dir_entry).align_page_down().to_virt());
            page_table[tab_index] = pte;
            kmem_free_page(page_table);
            return;
        }
        // normal path
        memset(page_table, 0, 4096);
        dir_entry = phys_t::from_kmem(page_table).value() | pde_flags;
        page_table[tab_index] = pte;
    }
}

void memory::init_page_allocator() {
    // allocate two initial page tables
    free_page_table = phys_t::from_kmem(&_kernel_end).align_page_up();
    phys_t first_page_table {free_page_table.value() + 4096};
    // set kmem and hmem beginning
    kmem_phys_end = phys_t(free_page_table.value() + 8192);
    hmem_phys_end = phys_t(ram_amount_start + ram_amount).align_page_down();
    hmem_end = 0;  // will be reduced to 0xFFFFF000

    memset((void *)first_page_table.to_virt(), 0, 4096);
    memset(first_page_directory, 0, 4096);
    first_page_directory[768] = encode_pte(first_page_table.value(), page_flag::present, page_flag::write);
    for (uint32_t addr = 0xC0000000; addr < (uint32_t)&_kernel_end + 8192; addr += 4096) {
        // map whole kernel as present + write
        constexpr uint32_t prwr = (uint32_t)page_flag::present | (uint32_t)page_flag::write;
        map_page((void *)addr, prwr, phys_t::from_kmem((void *)addr).value() | prwr);
    }

    uint32_t page_dir_phys = phys_t::from_kmem(first_page_directory).value();
    asm_set_cr3((void *)page_dir_phys);
}
