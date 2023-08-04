#include <kernel/memory/page_allocator.hpp>
#include <kernel/memory/multiboot.hpp>
#include <kernel/util.hpp>
#include <kernel/util/string.hpp>
#include <kernel/util/asm_wrap.hpp>
#include <kernel/logging.hpp>
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
// When kmem and hmem collide we crash... TODO add assert for this

// allocator globals

static phys_t kmem_phys_end;    // highest physical address used by kmem + 1 (e.g. 0x1000 instead of 0xFFF which is end of page), global
static phys_t kmem_region_end;  // end of the kmem region - either the amount of memory or 896MB, whichever is lower. Top 128MB reserved to map hmem per process.
                                // page tables within kmem_region_end should NOT be freed
static phys_t hmem_phys_end;    // lowest  physical address used by hmem, global
// highest virtual address used by kmem is kmem_phys_end.to_virt()

static phys_t hmem_phys_list;   // hmem allocator uses a linked list of pages. 0 if empty. otherwise a physical addr which also contains the next in the list.

// buddies are allocated in advance according to the amount of RAM
struct buddy;
static buddy *buddy_array;      // global
static size_t buddy_array_pos;  // global
static size_t buddy_array_size;  // global

// buddy allocator struct, 512k per buddy
// can fit 128 buddies in a page which map 64MB
struct buddy_data {
    uint32_t k4[4];    // 128 bits of 4k, 1 if free
    uint32_t k8[2];    // 64 bits of 8k
    uint32_t k16;      // 32 bits of 16k
    uint16_t k32;      // 16 bits of 32k
    buddy *prev_k4;
    buddy *next_k4;
    buddy *prev_k8;
    buddy *next_k8;
    buddy *prev_k16;
    buddy *next_k16;
    buddy *prev_k32;
    buddy *next_k32;
};

struct {
    buddy_data first_k4;   // start of linked list, use next_N, which is either a free buddy or nullptr
    buddy_data first_k8;
    buddy_data first_k16;
    buddy_data first_k32;

    template <size_t N>
    inline buddy& first_N() {
        if constexpr (N == 4096) {
            return reinterpret_cast<buddy&>(first_k4);
        } else if constexpr (N == 8192) {
            return reinterpret_cast<buddy&>(first_k8);
        } else if constexpr (N == 16384) {
            return reinterpret_cast<buddy&>(first_k16);
        } else if constexpr (N == 32768) {
            return reinterpret_cast<buddy&>(first_k32);
        }
    }
} kmem_allocator;  // global

struct buddy : buddy_data {
    template <size_t N>
    inline buddy *next_N() {
        if constexpr (N == 4096) {
            return next_k4;
        } else if constexpr (N == 8192) {
            return next_k8;
        } else if constexpr (N == 16384) {
            return next_k16;
        } else if constexpr (N == 32768) {
            return next_k32;
        }
    }

private:
    // returns 0 if was already allocated
    uint32_t mark_alloc_k4(uint32_t index) {
        uint32_t arr_index = index >> 5;
        uint32_t bit = 1 << (index & 0x1F);
        uint32_t res = k4[arr_index] & bit;
        k4[arr_index] &= ~res;
        if (k4[arr_index] == 0) {
            if (k4[0] == 0 && k4[1] == 0 && k4[2] == 0 && k4[3] == 0) {
                // unlink from k4
                if (prev_k4 != nullptr) prev_k4->next_k4 = next_k4;
                if (next_k4 != nullptr) next_k4->prev_k4 = prev_k4;
                next_k4 = nullptr;
                prev_k4 = nullptr;
            }
        }
        return res;
    }
    void mark_free_k4(uint32_t index) {
        if (prev_k4 == nullptr && k4[0] == 0 && k4[1] == 0 && k4[2] == 0 && k4[3] == 0) {
            // link to k4
            next_k4 = kmem_allocator.first_k4.next_k4;
            if (next_k4 != nullptr) next_k4->prev_k4 = this;
            prev_k4 = static_cast<buddy *>(&kmem_allocator.first_k4);
            kmem_allocator.first_k4.next_k4 = this;
        }
        uint32_t arr_index = index >> 5;
        uint32_t bit = 1 << (index & 0x1F);
        k4[arr_index] |= bit;
    }

    uint32_t mark_alloc_k8(uint32_t index) {
        uint32_t arr_index = index >> 5;
        uint32_t bit = 1 << (index & 0x1F);
        uint32_t res = k8[arr_index] & bit;
        k8[arr_index] &= ~res;
        if (k8[arr_index] == 0) {
            if (k8[0] == 0 && k8[1] == 0) {
                // unlink from k8
                if (prev_k8 != nullptr) prev_k8->next_k8 = next_k8;
                if (next_k8 != nullptr) next_k8->prev_k8 = prev_k8;
                next_k8 = nullptr;
                prev_k8 = nullptr;
            }
        }
        return res;
    }
    void mark_free_k8(uint32_t index) {
        if (prev_k8 == nullptr && k8[0] == 0 && k8[1] == 0) {
            // link to k8
            next_k8 = kmem_allocator.first_k8.next_k8;
            if (next_k8 != nullptr) next_k8->prev_k8 = this;
            prev_k8 = static_cast<buddy *>(&kmem_allocator.first_k8);
            kmem_allocator.first_k8.next_k8 = this;
        }
        uint32_t arr_index = index >> 5;
        uint32_t bit = 1 << (index & 0x1F);
        k8[arr_index] |= bit;
    }

    uint32_t mark_alloc_k16(uint32_t index) {
        uint32_t bit = 1 << index;
        uint32_t res = k16 & bit;
        k16 &= ~res;
        if (k16 == 0) {
            // unlink from k16
            if (prev_k16 != nullptr) prev_k16->next_k16 = next_k16;
            if (next_k16 != nullptr) next_k16->prev_k16 = prev_k16;
            next_k16 = nullptr;
            prev_k16 = nullptr;
        }
        return res;
    }
    void mark_free_k16(uint32_t index) {
        if (prev_k16 == nullptr && k16 == 0) {
            // link to k16
            next_k16 = kmem_allocator.first_k16.next_k16;
            if (next_k16 != nullptr) next_k16->prev_k16 = this;
            prev_k16 = static_cast<buddy *>(&kmem_allocator.first_k16);
            kmem_allocator.first_k16.next_k16 = this;
        }
        uint32_t bit = 1 << index;
        k16 |= bit;
    }

    uint16_t mark_alloc_k32(uint32_t index) {
        uint32_t bit = 1 << index;
        uint32_t res = k32 & bit;
        k32 &= ~res;
        if (k32 == 0) {
            // unlink from k32
            if (prev_k32 != nullptr) prev_k32->next_k32 = next_k32;
            if (next_k32 != nullptr) next_k32->prev_k32 = prev_k32;
            next_k32 = nullptr;
            prev_k32 = nullptr;
        }
        return res;
    }
    void mark_free_k32(uint32_t index) {
        if (prev_k32 == nullptr && k32 == 0) {
            // link to k32
            next_k32 = kmem_allocator.first_k32.next_k32;
            if (next_k32 != nullptr) next_k32->prev_k32 = this;
            prev_k32 = static_cast<buddy *>(&kmem_allocator.first_k32);
            kmem_allocator.first_k32.next_k32 = this;
        }
        uint32_t bit = 1 << index;
        k32 |= bit;
    }

public:
    template <size_t N>
    inline void *alloc() {
        uint32_t my_index = this - buddy_array;
        uint32_t my_first_page = ((uint32_t)buddy_array) + buddy_array_size + (my_index << 19);

        if constexpr (N == 4096) {

            uint32_t arr_index = 4;
            if (k4[0] != 0) {
                arr_index = 0;
            } else if (k4[1] != 0) {
                arr_index = 1;
            } else if (k4[2] != 0) {
                arr_index = 2;
            } else if (k4[3] != 0) {
                arr_index = 3;
            }
            kassert(arr_index != 4);

            uint32_t bit = __builtin_ctz(k4[arr_index]);
            uint32_t index = (arr_index << 5) + bit;

            mark_alloc_k4(index);
            if (mark_alloc_k8(index >> 1)) {
                if (mark_alloc_k16(index >> 2)) {
                    mark_alloc_k32(index >> 3);
                }
            }

            return (void *)(my_first_page + (index << 12));

        } else if constexpr (N == 8192) {

            uint32_t arr_index = 2;
            if (k8[0] != 0) {
                arr_index = 0;
            } else if (k8[1] != 0) {
                arr_index = 1;
            }
            kassert(arr_index != 2);

            uint32_t bit = __builtin_ctz(k8[arr_index]);
            uint32_t index = (arr_index << 5) + bit;

            mark_alloc_k4((index << 1) + 0);
            mark_alloc_k4((index << 1) + 1);

            mark_alloc_k8(index);
            if (mark_alloc_k16(index >> 1)) {
                mark_alloc_k32(index >> 2);
            }

            return (void *)(my_first_page + (index << 13));

        } else if constexpr (N == 16384) {

            uint32_t bit = __builtin_ctz(k16);

            mark_alloc_k4((bit << 2) + 0);
            mark_alloc_k4((bit << 2) + 1);
            mark_alloc_k4((bit << 2) + 2);
            mark_alloc_k4((bit << 2) + 3);
            mark_alloc_k8((bit << 1) + 0);
            mark_alloc_k8((bit << 1) + 1);

            mark_alloc_k16(bit);
            mark_alloc_k32(bit >> 1);

            return (void *)(my_first_page + (bit << 14));

        } else if constexpr (N == 32768) {

            uint32_t bit = __builtin_ctz(k32);

            mark_alloc_k32(bit);
            for (int i = 0; i < 8; i++) {
                mark_alloc_k4((bit << 3) + i);
            }
            mark_alloc_k8((bit << 2) + 0);
            mark_alloc_k8((bit << 2) + 1);
            mark_alloc_k8((bit << 2) + 2);
            mark_alloc_k8((bit << 2) + 3);
            mark_alloc_k16((bit << 1) + 0);
            mark_alloc_k16((bit << 1) + 1);

            return (void *)(my_first_page + (bit << 15));

        }
    }

private:
    template <size_t N>
    void free_down(uint32_t index) {
        index <<= 1;
        if constexpr (N == 4096) {
            // nop
        } else if constexpr (N == 8192) {
            mark_free_k4(index);
            mark_free_k4(index ^ 1);
        } else if constexpr (N == 16384) {
            mark_free_k8(index);
            free_down<8192>(index);
            mark_free_k8(index ^ 1);
            free_down<8192>(index ^ 1);
        } else if constexpr (N == 32768) {
            mark_free_k16(index);
            free_down<16384>(index);
            mark_free_k16(index ^ 1);
            free_down<16384>(index ^ 1);
        }
    }

    template <size_t N>
    void free_up(uint32_t index) {
        if constexpr (N == 4096) {

            mark_free_k4(index);
            // check if buddy is free
            index ^= 1;
            uint32_t arr_index = index >> 5;
            uint32_t bit = 1 << (index & 0x1F);

            if (k4[arr_index] & bit) {
                // both are free
                free_up<8192>(index >> 1);
            }

        } else if constexpr (N == 8192) {

            mark_free_k8(index);
            // check if buddy is free
            index ^= 1;
            uint32_t arr_index = index >> 5;
            uint32_t bit = 1 << (index & 0x1F);

            if (k8[arr_index] & bit) {
                // both are free
                free_up<16384>(index >> 1);
            }

        } else if constexpr (N == 16384) {

            mark_free_k16(index);
            // check if buddy is free
            index ^= 1;
            uint32_t bit = 1 << index;

            if (k16 & bit) {
                // both are free
                free_up<32768>(index >> 1);
            }

        } else if constexpr (N == 32768) {

            mark_free_k32(index);

        }
    }

public:
    template <size_t N>
    inline void free(uint32_t index) {
        free_up<N>(index);
        free_down<N>(index);
    }
};

static constexpr size_t buddies_in_page = 4096 / sizeof(buddy);
static_assert(sizeof(buddy) == 64, "wrong size buddy");
static_assert(buddies_in_page == 64, "wrong amount of buddies, buddy");

__attribute__((aligned(4096)))
static uint32_t first_page_directory[1024];

template <size_t N>
static void *buddy_alloc() {
    buddy &first = kmem_allocator.first_N<N>();
    buddy *next = first.next_N<N>();
    if (next != nullptr) [[likely]] {
        void *res = next->alloc<N>();
        memset(res, 0x41, N);  // TODO remove, debugging tool
        return res;
    } else {
        return nullptr;
    }
}

static void kmem_buddy_new_no_alloc() {
    size_t pos = buddy_array_pos++;
    kassert((pos * sizeof(buddy)) < buddy_array_size);
    memset(&buddy_array[pos], 0xFF, sizeof(buddy));

    buddy_array[pos].prev_k4 = static_cast<buddy*>(&kmem_allocator.first_k4);
    buddy_array[pos].next_k4 = kmem_allocator.first_k4.next_k4;
    kmem_allocator.first_k4.next_k4 = &buddy_array[pos];

    buddy_array[pos].prev_k8 = static_cast<buddy*>(&kmem_allocator.first_k8);
    buddy_array[pos].next_k8 = kmem_allocator.first_k8.next_k8;
    kmem_allocator.first_k8.next_k8 = &buddy_array[pos];

    buddy_array[pos].prev_k16 = static_cast<buddy*>(&kmem_allocator.first_k16);
    buddy_array[pos].next_k16 = kmem_allocator.first_k16.next_k16;
    kmem_allocator.first_k16.next_k16 = &buddy_array[pos];

    buddy_array[pos].prev_k32 = static_cast<buddy*>(&kmem_allocator.first_k32);
    buddy_array[pos].next_k32 = kmem_allocator.first_k32.next_k32;
    kmem_allocator.first_k32.next_k32 = &buddy_array[pos];
}

static void _map_page(void *virt, uint32_t pde_flags, uint32_t pte, uint32_t *pd = first_page_directory);
static void kmem_buddy_new() {
    kmem_buddy_new_no_alloc();
    size_t pos = buddy_array_pos - 1;
    uint32_t start = ((uint32_t)buddy_array) + buddy_array_size + (pos << 19);
    uint32_t end = start + (1 << 19);
    for (uint32_t addr = start; addr < end; addr += 4096) {
        constexpr uint32_t prwr = (uint32_t)page_flag::present | (uint32_t)page_flag::write;
        _map_page((void *)addr, prwr, prwr | phys_t::from_kmem((void *)addr).value());
    }
    kassert(kmem_phys_end.to_virt() == start);
    kmem_phys_end = phys_t(kmem_phys_end.value() + (1 << 19));
    kassert(kmem_phys_end.value() < kmem_region_end.value());
}

void *memory::kmem_alloc_4k() {
    scoped_intlock lock;  // block interrupts

    void *result = buddy_alloc<4096>();
    if (result == nullptr) [[unlikely]] {
        kmem_buddy_new();
        result = buddy_alloc<4096>();
        kassert(result != nullptr);
    }
    return result;
}

void *memory::kmem_alloc_8k() {
    scoped_intlock lock;  // block interrupts

    void *result = buddy_alloc<8192>();
    if (result == nullptr) [[unlikely]] {
        kmem_buddy_new();
        result = buddy_alloc<8192>();
        kassert(result != nullptr);
    }
    return result;
}

void *memory::kmem_alloc_16k() {
    scoped_intlock lock;  // block interrupts

    void *result = buddy_alloc<16384>();
    if (result == nullptr) [[unlikely]] {
        kmem_buddy_new();
        result = buddy_alloc<16384>();
        kassert(result != nullptr);
    }
    return result;
}

void *memory::kmem_alloc_32k() {
    scoped_intlock lock;  // block interrupts

    void *result = buddy_alloc<32768>();
    if (result == nullptr) [[unlikely]] {
        kmem_buddy_new();
        result = buddy_alloc<32768>();
        kassert(result != nullptr);
    }
    return result;
}

void memory::kmem_free_4k(void *ptr) {
    scoped_intlock lock;  // block interrupts

    uint32_t buddy_rel_pos = (uint32_t)ptr - ((uint32_t)buddy_array) - buddy_array_size;
    buddy &bud = buddy_array[buddy_rel_pos >> 19];
    kassert((buddy_rel_pos & 0x0FFF) == 0);
    uint32_t alloc_index = (buddy_rel_pos & 0x7FFFF) >> 12;  // 0x7FFFF is 1<<19 - 1, >>12 is for 4k division
    bud.free<4096>(alloc_index);
}

void memory::kmem_free_8k(void *ptr) {
    scoped_intlock lock;  // block interrupts

    uint32_t buddy_rel_pos = (uint32_t)ptr - ((uint32_t)buddy_array) - buddy_array_size;
    buddy &bud = buddy_array[buddy_rel_pos >> 19];
    kassert((buddy_rel_pos & 0x1FFF) == 0);
    uint32_t alloc_index = (buddy_rel_pos & 0x7FFFF) >> 13;
    bud.free<8192>(alloc_index);
}

void memory::kmem_free_16k(void *ptr) {
    scoped_intlock lock;  // block interrupts

    uint32_t buddy_rel_pos = (uint32_t)ptr - ((uint32_t)buddy_array) - buddy_array_size;
    buddy &bud = buddy_array[buddy_rel_pos >> 19];
    kassert((buddy_rel_pos & 0x3FFF) == 0);
    uint32_t alloc_index = (buddy_rel_pos & 0x7FFFF) >> 14;
    bud.free<16384>(alloc_index);
}

void memory::kmem_free_32k(void *ptr) {
    scoped_intlock lock;  // block interrupts

    uint32_t buddy_rel_pos = (uint32_t)ptr - ((uint32_t)buddy_array) - buddy_array_size;
    buddy &bud = buddy_array[buddy_rel_pos >> 19];
    kassert((buddy_rel_pos & 0x7FFF) == 0);
    uint32_t alloc_index = (buddy_rel_pos & 0x7FFFF) >> 15;
    bud.free<32768>(alloc_index);
}

reg_t memory::new_page_directory() {
    uint32_t *page_table = static_cast<uint32_t *>(kmem_alloc_4k());  // TODO allocate this page table in hmem
    memcpy(page_table, first_page_directory, 4096);
    return reinterpret_cast<reg_t>(phys_t::from_kmem(page_table).value());
}

static void _map_page(void *virt, uint32_t pde_flags, uint32_t pte, uint32_t *pd /* default first_page_directory*/) {
    scoped_intlock lock;  // block interrupts
    kassert(((uint32_t)virt & 0xFFF) == 0);  // make sure address is page aligned

    uint32_t dir_index = (uint32_t)virt >> 22;
    uint32_t tab_index = (uint32_t)virt >> 12 & 0x03FF;

    uint32_t &dir_entry = pd[dir_index];
    uint32_t *page_table;

    if (dir_entry & (uint32_t)page_flag::present) {
        // page table already exists!
        page_table = (uint32_t *)(void *)(phys_t(dir_entry).align_page_down().to_virt());
        page_table[tab_index] = pte;
    } else {
        // need to allocate new page table
        page_table = (uint32_t *)kmem_alloc_4k();
        if (dir_entry & (uint32_t)page_flag::present) [[unlikely]] {
            // kmem_alloc_4k created a page table for us, so we don't need this one anymore
            kmem_free_4k(page_table);
            page_table = (uint32_t *)(void *)(phys_t(dir_entry).align_page_down().to_virt());
            page_table[tab_index] = pte;
            return;
        }
        // normal path
        memset(page_table, 0, 4096);
        dir_entry = phys_t::from_kmem(page_table).value() | pde_flags;
        page_table[tab_index] = pte;
    }
}

void memory::map_user_page(void *virt, phys_t phys, bool writable) {
    constexpr uint32_t prwr = (uint32_t)page_flag::present | (uint32_t)page_flag::write | (uint32_t)page_flag::user;
    uint32_t pte = phys.value() | (uint32_t)page_flag::present | (uint32_t)page_flag::user;

    if (writable)
        pte |= (uint32_t)page_flag::write;

    uint32_t cr3;
    asm volatile("movl %%cr3, %0" : "=r"(cr3) ::);
    _map_page(virt, prwr, pte, reinterpret_cast<uint32_t *>(phys_t(cr3).to_virt()));
}

// hmem linked list physical pages allocator
// do NOT call from interrupt context!
phys_t memory::hmem_alloc_page() {
    kassert_not_interrupt;
    scoped_preemptlock lock;

    if (hmem_phys_list.value() == 0) {
        // no value in linked list, must reduce hmem_phys_end
        hmem_phys_end = phys_t(hmem_phys_end.value() - 4096);
        kassert(kmem_phys_end.value() < hmem_phys_end.value());
        return phys_t(hmem_phys_end);
    } else {
        // have a value in the linked list - return current, and swap it with the next free page
        phys_t result = hmem_phys_list;
        scoped_hmem_mapping map {result};
        uint32_t *contents = reinterpret_cast<uint32_t *>(map.value());
        hmem_phys_list = phys_t(*contents);
        return result;
    }
    return phys_t(0);
}

void memory::hmem_free_page(phys_t addr) {
    kassert_not_interrupt;
    scoped_preemptlock lock;

    kassert(addr.value() >= hmem_phys_end.value());
    // add addr to hmem_phys_list
    if (hmem_phys_list.value() == 0) {
        hmem_phys_list = addr;
    } else {
        scoped_hmem_mapping map {addr};
        uint32_t *contents = reinterpret_cast<uint32_t *>(map.value());
        *contents = hmem_phys_list.value();
        hmem_phys_list = addr;
    }
}

memory::scoped_hmem_mapping::scoped_hmem_mapping(phys_t addr) {
    kassert(scheduler::current_task != 0);
    char *&hmem_end = scheduler::get_current_task_internal()->hmem_end;
    hmem_end -= 4096;
    m_virt = hmem_end;

    constexpr uint32_t prwr = (uint32_t)page_flag::present | (uint32_t)page_flag::write;
    uint32_t pte = addr.value() | prwr;

    uint32_t dir_index = (uint32_t)m_virt >> 22;
    kassert(dir_index == 0x03FF);  // all mappings should fit in the last page directory (1024 mapped pages at once)
    uint32_t tab_index = (uint32_t)m_virt >> 12 & 0x03FF;

    reg_t cr3;
    asm volatile("movl %%cr3, %0" : "=r"(cr3) ::);
    uint32_t *page_dir = reinterpret_cast<uint32_t *>(phys_t(cr3).to_virt());
    uint32_t &dir_entry = page_dir[dir_index];
    kassert(dir_entry & (uint32_t)page_flag::present);
    uint32_t *page_table = (uint32_t *)(void *)(phys_t(dir_entry).align_page_down().to_virt());
    page_table[tab_index] = pte;
    asm volatile("invlpg (%0)" :: "r"(m_virt) : "memory");  // might have been used previously
}

memory::scoped_hmem_mapping::~scoped_hmem_mapping() {
    kassert(scheduler::current_task != 0);
    char *&hmem_end = scheduler::get_current_task_internal()->hmem_end;
    kassert(hmem_end == m_virt)
    hmem_end += 4096;
    m_virt = 0;
}

void memory::init_page_allocator() {
    // pre-allocate all required buddies
    // number of pages in the system
    uint32_t total_page_count = (ram_amount + 4095) >> 12;
    // each buddy is for 128 pages
    uint32_t total_buddy_count = (total_page_count + 127) >> 7;
    // there are 64 buddies in a page
    uint32_t total_buddy_page_count = (total_buddy_count + 63) >> 6;

    phys_t buddy_memory_phys = phys_t::from_kmem(&_kernel_end).align_page_up();
    buddy_array = (buddy *)buddy_memory_phys.to_virt();
    uint32_t buddy_array_end = (((uint32_t)buddy_array + (total_buddy_page_count << 12) + 32767) >> 15) << 15;  // align up to 32k, so that allocations are aligned
    buddy_array_size = buddy_array_end - (uint32_t)buddy_array;

    buddy_array_pos = 0;
    phys_t buddy_memory_end_phys = phys_t(buddy_memory_phys.value() + buddy_array_size);
    memset(&kmem_allocator, 0, sizeof(kmem_allocator));
    kmem_buddy_new_no_alloc();  // first buddy allocation will come with the rest of the kernel

    // set kmem and hmem beginning - already WITH first buddy
    kmem_phys_end = phys_t(buddy_memory_end_phys.value() + (1 << 19));

    // upper limit for kmem region is 896MB, otherwise based on RAM size
    kmem_region_end = phys_t(buddy_memory_end_phys.value() + (total_page_count << 12));
    if (kmem_region_end.value() > 0x38000000)
        kmem_region_end = phys_t(0x38000000);

    hmem_phys_end = phys_t(ram_amount_start + ram_amount).align_page_down();
    hmem_phys_list = phys_t(0);

    uint32_t initially_mapped = kmem_phys_end.to_virt();

    memset(first_page_directory, 0, 4096);
    for (uint32_t addr = 0xC0000000; addr < initially_mapped; addr += 4096) {
        // map whole kernel as present + write
        constexpr uint32_t prwr = (uint32_t)page_flag::present | (uint32_t)page_flag::write;
        _map_page((void *)addr, prwr, phys_t::from_kmem((void *)addr).value() | prwr);
    }

    uint32_t page_dir_phys = phys_t::from_kmem(first_page_directory).value();
    asm_set_cr3((void *)page_dir_phys);

    // create page tables for whole kmem region
    for (uint32_t addr = ((initially_mapped + (1 << 22) - 1) >> 22) << 22; addr < kmem_region_end.to_virt(); addr += (1 << 22)) {
        uint32_t dir_index = (uint32_t)addr >> 22;
        uint32_t &dir_entry = first_page_directory[dir_index];
        kassert((dir_entry & (uint32_t)page_flag::present) == 0);

        // allocate new page table
        uint32_t *page_table = (uint32_t *)kmem_alloc_4k();
        memset(page_table, 0, 4096);
        dir_entry = phys_t::from_kmem(page_table).value() | (uint32_t)page_flag::present | (uint32_t)page_flag::write;
    }
}
