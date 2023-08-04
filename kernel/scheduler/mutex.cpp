#include <kernel/scheduler/mutex.hpp>
#include <kernel/util/lock.hpp>

void scheduler::concurrency::mutex::lock() {
    // not applicable for interrupt context
    kassert_not_interrupt;
    // lock preemption
    scheduler::preempt_up();

    if (m_owner == nullptr) {
        // uncontended case
        m_owner = scheduler::current_task;
        // unlock preemption
        scheduler::preempt_down();
    } else {
        // contended case - start blocking, then enable preemption and yield to the scheduler
        scheduler::current_task->blocking.block_on(&m_list);
        // unlock preemption and yield
        scheduler::preempt_down();
        scheduler::yield();
    }
}

void scheduler::concurrency::mutex::unlock() {
    // not applicable for interrupt context
    kassert(!interrupts::is_interrupt_context());
    kassert(m_owner == scheduler::current_task);
    scoped_preemptlock internal_lock;

    if (!m_list.lonely()) {
        // uncontended case
        m_owner = nullptr;
    } else {
        // contended case
        task_blocking *waiter_blocking_subsystem = m_list.get_next();
        waiter_blocking_subsystem->unblock();
        m_owner = task::from(waiter_blocking_subsystem);
    }
}
