#pragma once
#include <kernel/util.hpp>

namespace interrupts {
    // if modifying, please look at scheduler::yield too - it generates this kind of struct
    struct __attribute__((packed)) interrupt_args {
        reg_t cr3;
        reg_t ebp;
        reg_t edi;
        reg_t esi;
        reg_t edx;
        reg_t ecx;
        reg_t ebx;
        reg_t eax;
        reg_t interrupt_number;
        reg_t error_code;
        reg_t eip;
        reg_t cs;
        reg_t eflags;
    };

    void reduce_interrupt_depth();  // called by e.g. scheduler
    bool is_interrupt_context();  // is currently inside an interrupt?
    int get_interrupt_context_depth();  // current depth of interrupt context
    void initialize();
    void start();  // start getting interrupts
    void register_handler(uint interrupt, void (*interrupt_handler)(interrupt_args &args));

    inline void sti() {
        asm volatile("sti" ::: "memory");
    }
    inline void cli() {
        asm volatile("cli" ::: "memory");
    }
}
