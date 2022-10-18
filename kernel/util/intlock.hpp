#pragma once
#include <kernel/util.hpp>

// Scoped Interrupt Lock - saves interrupt flag and restores it
struct scoped_intlock {
public:
    inline scoped_intlock() {
        asm volatile("pushf ; pop %0" : "=rm" (m_flags) : /* no input */ : "memory");
        asm volatile("cli");
    }
    inline ~scoped_intlock() {
        if (m_flags & (1 << 9)) {
            // Interrupt Flag was set
            asm volatile("sti");
        }
    }

    // move semantics
    inline scoped_intlock(scoped_intlock&& other) : m_flags(other.m_flags) {
        other.m_flags = 0;  // shouldn't set interrupts back
    }
    inline scoped_intlock &operator=(scoped_intlock&& other) {
        m_flags = other.m_flags;
        other.m_flags = 0;
        return *this;
    }

    scoped_intlock(const scoped_intlock &other) = delete;
    inline scoped_intlock &operator=(const scoped_intlock &other) = delete;
private:
    reg_t m_flags;
};
