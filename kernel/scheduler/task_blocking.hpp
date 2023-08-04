#pragma once
#include <kernel/util/ds/list.hpp>
#include <kernel/scheduler/init.hpp>

namespace scheduler {
    // blocked tasks are part of a linked list
    struct task_blocking final : private ds::intrusive_doubly_linked_node<task_blocking> {
        // needed for casting within the link
        friend ds::intrusive_doubly_linked_node<task_blocking>;
    public:
        // am i a member of a larger list blocking for something?
        inline bool is_blocked() {
            return !lonely();
        }

        // add myself to a blocking list
        inline void block_on(ds::intrusive_doubly_linked_node<task_blocking> *list) {
            list->get_prev()->add_after_self(this);
        }

        inline void unblock() {
            kassert(is_blocked());
            unlink();
        }
    };
}
