#pragma once
#include <kernel/util.hpp>
#include <kernel/util/ds/refcount.hpp>
#include <kernel/util/ds/rbtree.hpp>

namespace fs {
    struct file_desc;
}
namespace scheduler {
    struct task;
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

    // currently owned by a task
    struct virtual_memory {
        friend scheduler::task;
    private:
        ds::rbtree<vm_area> m_areas;

    private:
        // should only be called (implicitly) in task constructor
        inline virtual_memory() : m_areas{} {};
        // destroys all vm areas - should only be called in task destructor
        ~virtual_memory();

    public:
        static void release(virtual_memory *obj);
    };
}
