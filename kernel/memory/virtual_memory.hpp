#pragma once
#include <kernel/util.hpp>
#include <kernel/util/ds/refcount.hpp>
#include <kernel/util/ds/rbtree.hpp>

namespace fs {
    struct file_desc;
}

namespace memory {
    struct vm_area : ds::intrusive_rb_node<vm_area> {
        // start of range (page aligned)
        reg_t m_start;
        // end of range (page aligned)
        reg_t m_end;
        // offset within file
        reg_t m_file_offset;
        // file which it maps
        fs::file_desc *m_file;
    };

    struct virtual_memory : ds::intrusive_refcount {
    private:
        ds::rbtree<vm_area> m_areas;

    public:
        static void release(virtual_memory *obj);
        inline virtual_memory(void) : m_areas{} {}
    };
}
