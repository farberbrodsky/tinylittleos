#pragma once
#include <kernel/util.hpp>
#include <kernel/util/lock.hpp>

namespace memory {
    struct phys_t {
    public:
        // normal constructor
        inline constexpr phys_t(void) : m_addr(0) {}
        // EXPLICIT constructor from uint32_t when needed
        inline constexpr explicit phys_t(uint32_t addr) : m_addr(addr) {}

        // copy constructors
        inline constexpr phys_t(const phys_t &other) : m_addr(other.m_addr) {}
        inline constexpr phys_t &operator=(const phys_t &other) {
            m_addr = other.m_addr;
            return *this;
        }

        // equality
        inline constexpr bool operator==(const phys_t &other) const {
            return m_addr == other.m_addr;
        }

        // EXPLICIT conversion to uint32_t when needed
        inline constexpr explicit operator uint32_t() {
            return m_addr;
        }
        inline constexpr uint32_t value() {
            return m_addr;
        }

        // custom utility functions
        inline constexpr phys_t align_page_down() {
            return phys_t((m_addr >> 12) << 12);
        }

        inline constexpr phys_t align_page_up() {
            return phys_t(((m_addr + 4095) >> 12) << 12);
        }

        static inline phys_t from_kmem(void *virt) {
            return phys_t(reinterpret_cast<uint32_t>(virt) - 0xC0000000);
        }

        inline constexpr uint32_t to_virt() const {
            return m_addr + 0xC0000000;
        }

    private:
        uint32_t m_addr;
    };

    enum class page_flag : uint32_t {
        present = 1,
        write = 1 << 1,
        user = 1 << 2,
        write_through = 1 << 3,
        cache_disable = 1 << 4,
        accessed = 1 << 5,
        dirty = 1 << 6,  // PTE only
        global = 1 << 8  // PTE only
    };

    void init_page_allocator();
    void *kmem_alloc_4k();
    void *kmem_alloc_8k();
    void *kmem_alloc_16k();
    void *kmem_alloc_32k();
    void kmem_free_4k(void *ptr);
    void kmem_free_8k(void *ptr);
    void kmem_free_16k(void *ptr);
    void kmem_free_32k(void *ptr);

    // used when creating a new process
    reg_t new_page_directory(void);

    // hmem has only one allocation size (4k), and is based on a simple linked list
    // do NOT call from interrupt context!
    phys_t hmem_alloc_page();
    void hmem_free_page(phys_t addr);

    // maps an hmem mapping into view
    struct scoped_hmem_mapping {
        scoped_hmem_mapping(phys_t);
        ~scoped_hmem_mapping();

        // returns the address of the page in virtual memory
        inline void *value() {
            return m_virt;
        }

        // do not allow any sort of copying or moving
        scoped_hmem_mapping(const scoped_hmem_mapping &other) = delete;
        scoped_hmem_mapping(scoped_hmem_mapping &&other) = delete;
        scoped_hmem_mapping &operator=(const scoped_hmem_mapping &other) = delete;
        scoped_hmem_mapping &operator=(scoped_hmem_mapping &&other) = delete;

    private:
        void *m_virt;
    };

    template <size_t N>
    inline void *kmem_alloc_pages() {
        static_assert(N == 4096 || N == 8192 || N == 16384 || N == 32768, "Invalid size");
        if constexpr (N == 4096) {
            return kmem_alloc_4k();
        } else if constexpr (N == 8192) {
            return kmem_alloc_8k();
        } else if constexpr (N == 16384) {
            return kmem_alloc_16k();
        } else if constexpr (N == 32768) {
            return kmem_alloc_32k();
        }
    }

    template <size_t N>
    inline void kmem_free_pages(void *ptr) {
        static_assert(N == 4096 || N == 8192 || N == 16384 || N == 32768, "Invalid size");
        if constexpr (N == 4096) {
            kmem_free_4k(ptr);
        } else if constexpr (N == 8192) {
            kmem_free_8k(ptr);
        } else if constexpr (N == 16384) {
            kmem_free_16k(ptr);
        } else if constexpr (N == 32768) {
            kmem_free_32k(ptr);
        }
    }

    // called from a process context - virt should not be already mapped, it will override
    void map_user_page(void *virt, phys_t phys, bool writable);
}
