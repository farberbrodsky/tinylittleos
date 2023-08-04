#include <kernel/scheduler/mutex.hpp>
#include <kernel/util/lock.hpp>

void scheduler::concurrency::mutex::lock() {
    // not applicable for interrupt context
    kassert_not_interrupt;
    // lock preemption
    scheduler::preempt_up();
    // not to be called by the owner
    kassert(m_owner != scheduler::current_task);

    if (m_owner == nullptr) {
        // uncontended case
        m_owner = scheduler::current_task;
        // unlock preemption
        scheduler::preempt_down();
    } else {
        // contended case - start blocking, then enable preemption and yield to the scheduler
        scheduler::current_task->blocking.block_on(&m_list);
        // unlock preemption and yield
        // we won't be resumed until we're unblocked
        scheduler::preempt_down();
        scheduler::yield();
        kassert(m_owner == scheduler::current_task);
    }
}

void scheduler::concurrency::mutex::unlock() {
    // not applicable for interrupt context
    kassert_not_interrupt;
    // lock preemption
    scoped_preemptlock internal_lock;
    // only called by owner
    kassert(m_owner == scheduler::current_task);

    if (m_list.lonely()) {
        // uncontended case
        m_owner = nullptr;
    } else {
        // contended case
        task_blocking *waiter_blocking_subsystem = m_list.get_next();
        waiter_blocking_subsystem->unblock();
        m_owner = task::from(waiter_blocking_subsystem);
    }
}
