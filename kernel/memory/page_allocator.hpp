#pragma once
#include <kernel/util.hpp>
#include <kernel/util/intlock.hpp>

namespace memory {
    struct phys_t {
    public:
        // normal constructor
        inline constexpr phys_t() : m_addr(0) {}
        // EXPLICIT constructor from uint32_t when needed
        inline constexpr explicit phys_t(uint32_t addr) : m_addr(addr) {}

        // copy constructors
        inline constexpr explicit phys_t(const phys_t &other) : m_addr(other.m_addr) {}
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
        present = 1 | 1 << 2,  // TODO TODO TODO poc only! Remove this
        write = 1 << 1,
        user = 1 << 2,
        write_through = 1 << 3,
        cache_disable = 1 << 4,
        accessed = 1 << 5,
        dirty = 1 << 6,  // PTE only
        global = 1 << 8  // PTE only
    };

    void init_page_allocator();
    void *kmem_alloc_page();
    void kmem_free_page(void *addr);
}
