#pragma once
#include <kernel/scheduler/task.hpp>

namespace scheduler {
    namespace concurrency {
        struct mutex {
        private:
            scheduler::task *m_owner;
            // tasks which are waiting for the mutex are linked to each other
            ds::intrusive_doubly_linked_node<scheduler::task_blocking> m_list;

        public:
            inline mutex() : m_owner {nullptr} {}
            inline ~mutex() { kassert(m_owner == nullptr); }
            void lock();
            void unlock();
        };
    }
}
