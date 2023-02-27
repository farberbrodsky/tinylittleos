#pragma once
#include <kernel/util.hpp>
#include <kernel/util/ds/list.hpp>
#include <kernel/interrupts/init.hpp>
#include <kernel/util/ds/refcount.hpp>

namespace scheduler {
    struct __attribute__((packed)) tss_entry {
        uint32_t prev_tss;
        uint32_t esp0;  // The stack pointer to load when changing to kernel mode
        uint32_t ss0;   // The stack segment to load when changing to kernel mode
        // Everything below here is unused.
        uint32_t esp1;  // esp and ss 1 and 2 would be used when switching to rings 1 or 2
        uint32_t ss1;
        uint32_t esp2;
        uint32_t ss2;
        uint32_t cr3;
        uint32_t eip;
        uint32_t eflags;
        uint32_t eax;
        uint32_t ecx;
        uint32_t edx;
        uint32_t ebx;
        uint32_t esp;
        uint32_t ebp;
        uint32_t esi;
        uint32_t edi;
        uint32_t es;
        uint32_t cs;
        uint32_t ss;
        uint32_t ds;
        uint32_t fs;
        uint32_t gs;
        uint32_t ldt;
        uint16_t trap;
        uint16_t iomap_base;
    };

    // tasks are in linked lists managed by scheduler
    struct task final : ds::intrusive_doubly_linked_node<task>, ds::intrusive_refcount {
        uint32_t pid;
        char *stack_pointer;  // should have an interrupts::interrupt_args at the top

        // create a task, to link it later
        // starts with one reference - it is assumed for the running of the task that the scheduler holds a reference and the running has a reference
        // do NOT call from interrupt context
        static task *allocate(void (*run)(void));
        // call to release a reference to a task
        // do NOT call from interrupt context
        static void release(task *obj);

    public:
        // please only construct using allocate
        task(uint32_t _pid, char *_stack_pointer);
    };

    // information which is at the highest address of the stack, and is only needed within the task
    struct task_internal {
        char *hmem_end;
    };

    inline task_internal *get_current_task_internal(void)
    {
        task_internal *current;
        asm volatile("andl %%esp,%0; ":"=r" (current) : "0" (~8191UL));
        // Do NOT change without changing create_kernel_stack
        // Assumes stack address is aligned to 8192
        return current;
    }

    extern tss_entry global_tss;
    extern task *volatile current_task;

    // initialize scheduling
    void initialize();
    // enter current task when ready
    void start();
    // add the task to the scheduler
    void link_task(task *t);
    // remove the task from the scheduler
    void unlink_task(task *t);

    void timeslice_passed(interrupts::interrupt_args &resume_info);  // called from interrupt context

    // for preemption locking
    void preempt_up();
    void preempt_down();
}
