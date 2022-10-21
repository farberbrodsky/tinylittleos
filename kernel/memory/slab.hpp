#pragma once
#include <kernel/util.hpp>
#include <kernel/util/ds/bitset.hpp>
#include <kernel/memory/page_allocator.hpp>

namespace memory {
    // untyped slab allocator - used to not duplicate allocator code for the same size
    template <size_t N>
    struct __slab_allocator {
        static_assert(N < 4000, "2 big 2 slab");

        // decide on page size
        static consteval size_t get_metadata_size(size_t page_size) {
            size_t bits_in_set = page_size / N;
            size_t bytes_for_set = ((bits_in_set + 31) / 32) * 4;
            return sizeof(void *) + bytes_for_set;
        }
        static consteval int waste_pct(size_t page_size) {
            if (page_size < N) return 1000;
            // rough estimate - does not account for metadata for keeping N objects
            int waste_bytes = page_size - (((page_size - get_metadata_size(page_size)) / N) * N);
            return (100 * waste_bytes) / page_size;
        }
        static consteval size_t get_page_size() {
            int w1 = waste_pct(4096);
            int w2 = waste_pct(8192);
            int w3 = waste_pct(16384);
            int w4 = waste_pct(32768);

            if (w4 < (w3 - 5)) {
                // have a good reason to use 32768
                return 32768;
            } else if (w3 < (w2 - 5)) {
                return 16384;
            } else if (w2 < (w1 - 5)) {
                return 8192;
            } else {
                return 4096;
            }
        }

        static constexpr inline size_t page_size = get_page_size();
        static constexpr inline size_t num_objects_in_page = (page_size - get_metadata_size(page_size)) / N;

        struct slab {
            ds::bitset<page_size / N> m_bits;  // 1 if free
            slab *m_next;
        };

        static_assert(sizeof(slab) == get_metadata_size(page_size), "unexpected slab size");

private:
        slab *m_free_list = nullptr;

public:
        void *allocate() {
            if (m_free_list == nullptr) [[unlikely]] {
                // need new slab
                slab *s = (slab *)kmem_alloc_pages<page_size>();  // slab is not constructed
                s->m_bits.clear_all();
                s->m_bits.set_all_until(num_objects_in_page);
                s->m_next = nullptr;
                m_free_list = s;
            }

            slab *s = m_free_list;
            uint32_t available_bit = s->m_bits.find_bit();
            s->m_bits.clear_bit(available_bit);

            if (s->m_bits.all_clear_until(num_objects_in_page)) {
                // unlink from free list
                m_free_list = s->m_next;
            }

            return (void *)((uint32_t)s + sizeof(slab) + (N * available_bit));
        }
        void free(void *ptr) {
            // go to beginning of page
            uint32_t page_start = (((uint32_t)ptr) / page_size) * page_size;
            slab *s = (slab *)page_start;
            // calculate address of matching bit
            uint32_t bit = ((uint32_t)ptr - page_start - sizeof(slab)) / N;

            if (s->m_bits.all_clear_until(num_objects_in_page)) {
                // link to free list
                s->m_next = m_free_list;
                m_free_list = s;
            }
            s->m_bits.set_bit(bit);
        }
    };

    template <class T>
    struct slab_allocator {
    private:
        __slab_allocator<sizeof(T)> internal;
    public:
        template <class... Args>
        inline T *allocate(Args... args) {
            void *ptr = internal.allocate();
            return new (ptr) T(args...);
        }

        inline void free(T *t) {
            t->~T();
            internal.free(t);
        }
    };
}
