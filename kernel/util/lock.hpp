#pragma once
#include <kernel/util.hpp>
#include <kernel/scheduler/init.hpp>
#include <kernel/scheduler/mutex.hpp>

// Scoped Interrupt Lock - saves interrupt flag and restores it
struct scoped_intlock {
public:
    inline scoped_intlock() {
        asm volatile("pushf ; pop %0" : "=rm" (m_flags) : /* no input */ : "memory");
        asm volatile("cli" ::: "memory");
    }
    inline ~scoped_intlock() {
        if (m_flags & (1 << 9)) {
            // Interrupt Flag was set
            asm volatile("sti");
        }
    }

    // move semantics
    inline scoped_intlock(scoped_intlock&& other) noexcept : m_flags(other.m_flags) {
        other.m_flags = 0;  // shouldn't set interrupts back
    }

    inline scoped_intlock &operator=(scoped_intlock&& other) = delete;
    scoped_intlock(const scoped_intlock &other) = delete;
    inline scoped_intlock &operator=(const scoped_intlock &other) = delete;
private:
    reg_t m_flags;
};

// Scoped Preemption Lock - disables preemption within scope, may be nested
struct scoped_preemptlock {
public:
    inline scoped_preemptlock() : m_moved {false} {
        scheduler::preempt_up();
    }
    inline ~scoped_preemptlock() {
        if (!m_moved) {
            scheduler::preempt_down();
        }
    }

    // move semantics
    inline scoped_preemptlock(scoped_preemptlock&& other) noexcept : m_moved {false} {
        other.m_moved = true;
        // don't increase again
    }

    inline scoped_preemptlock &operator=(scoped_preemptlock&& other) = delete;
    scoped_preemptlock(const scoped_preemptlock &other) = delete;
    inline scoped_preemptlock &operator=(const scoped_preemptlock &other) = delete;
private:
    bool m_moved;
};

// Scoped Mutex Lock - locks a mutex
struct scoped_mutex {
    inline scoped_mutex(scheduler::concurrency::mutex &mtx) : m_mtx {&mtx} {
        m_mtx->lock();
    }
    inline ~scoped_mutex() {
        if (m_mtx) {
            m_mtx->unlock();
        }
    }

    // move semantics
    inline scoped_mutex(scoped_mutex&& other) noexcept : m_mtx {other.m_mtx} {
        other.m_mtx = nullptr;
    }

    inline scoped_mutex &operator=(scoped_mutex&& other) = delete;
    scoped_mutex(const scoped_mutex &other) = delete;
    inline scoped_mutex &operator=(const scoped_mutex &other) = delete;
private:
    scheduler::concurrency::mutex *m_mtx;
};
