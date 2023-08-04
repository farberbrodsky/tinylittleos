#pragma once
#include <kernel/scheduler/init.hpp>
#include <kernel/scheduler/task_blocking.hpp>

namespace scheduler {
    struct task final : ds::intrusive_refcount {
    public:
        uint32_t pid;
        char *stack_pointer;  // should have an interrupts::interrupt_args at the top
        memory::virtual_memory vm;

    public:
        // subsystems
        task_scheduling scheduling;
        task_blocking blocking;

        // access from subsystems
        inline static task *from(task_scheduling *x) { return container_of(x, task, scheduling); }
        inline static task *from(task_blocking *x) { return container_of(x, task, blocking); }

    public:
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
}
