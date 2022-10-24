#pragma once
#include <kernel/util.hpp>
#include <kernel/memory/slab.hpp>
#include <kernel/util/ds/list.hpp>
#include <kernel/util/lock.hpp>

namespace ds {
    struct __ht_link : public intrusive_doubly_linked_node<__ht_link> {
        uint32_t key;
        void *obj;
        inline __ht_link() : key(0), obj(nullptr) {}
    };
    extern memory::slab_allocator<__ht_link> __hashtable_link_alloc;
    static_assert(sizeof(__ht_link) == 16, "hashtable link size");

    template <size_t N>
    struct hashtable {
        static_assert(__builtin_popcount(N) == 1, "N should be a power of two");
    private:
        __ht_link m_arr[N];

        uint32_t hash(uint32_t val) {
            return (val * 0x61C88647) & (N - 1);  // golden ratio constant, good for 2^N
        }

        __ht_link *lookup_internal(uint32_t key, uint32_t h) {
            for (__ht_link &link : m_arr[h]) {
                if (link.key == key) {
                    return &link;
                }
            }
            return nullptr;
        }

    public:
        // CAN NOT BE STATICALLY INITIALIZED! Because linked list is NOT all null
        inline hashtable() {}

        void insert(uint32_t key, void *value) {
            scoped_preemptlock preempt_lock;
            kassert(value != nullptr);
            uint32_t h = hash(key);

            if (m_arr[h].obj == nullptr) {
                m_arr[h].key = key;
                m_arr[h].obj = value;
            } else {
                __ht_link *link;
                {
                    scoped_intlock lock;
                    link = __hashtable_link_alloc.allocate();
                }
                link->key = key;
                link->obj = value;
                m_arr[h].add_after_self(link);
            }
        }

        bool remove(uint32_t key) {
            scoped_preemptlock preempt_lock;
            uint32_t h = hash(key);

            __ht_link *link = lookup_internal(key, h);
            if (link == nullptr) return false;

            if (link == &m_arr[h]) {
                // unlink is more complicated
                if (link->get_next() == link) {
                    link->obj = nullptr;
                } else {
                    __ht_link *next = link->get_next();
                    link->key = next->key;
                    link->obj = next->obj;
                    next->unlink();
                    {
                        scoped_intlock lock;
                        __hashtable_link_alloc.free(next);
                    }
                }
            } else {
                link->unlink();
                {
                    scoped_intlock lock;
                    __hashtable_link_alloc.free(link);
                }
            }

            return true;
        }

        void *lookup(uint32_t key) {
            scoped_preemptlock preempt_lock;
            __ht_link *link = lookup_internal(key, hash(key));
            if (link == nullptr) return nullptr;
            return link->obj;
        }

    public:
        hashtable(const hashtable &other) = delete;
        hashtable &operator=(const hashtable &other) = delete;
        hashtable(hashtable &&other) = delete;
        hashtable &operator=(hashtable &&other) = delete;
    };
}
