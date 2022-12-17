#include <kernel/scheduler/init.hpp>
#include <kernel/util/string.hpp>
#include <kernel/memory/page_allocator.hpp>
#include <kernel/util/asm_wrap.hpp>
#include <kernel/memory/slab.hpp>
#include <kernel/tty.hpp>

scheduler::tss_entry scheduler::global_tss;
static uint32_t preempt_counter = 0;  // global
static memory::slab_allocator<scheduler::task> task_allocator;

static scheduler::task *volatile current_task = 0;

static char *create_kernel_stack(reg_t eip, char *&stack_top) {
    char *stack = static_cast<char *>(memory::kmem_alloc_8k());
    stack_top = stack + 8192;
    stack_top -= sizeof(interrupts::interrupt_args);
    interrupts::interrupt_args *info = reinterpret_cast<interrupts::interrupt_args *>(stack_top);
    memset(info, 0, sizeof(interrupts::interrupt_args));
    info->eip = eip;
    info->cs = 0x08;
    // EFLAGS is 0 so interrupts are initially disabled
    return stack;
}

static void enter_task(char *stack) {
    scheduler::global_tss.esp0 = reinterpret_cast<reg_t>(stack);
    asm_enter_task(stack);
}

static void main_thread() {
    asm volatile("sti" ::: "memory");
    while (1) {
        {
            scoped_preemptlock lock;
            tty_driver::write("hello!\n");
        }
        for (int i = 0; i < 300000; i++)
            asm volatile("pause" ::: "memory");
    }
}

static void second_thread() {
    asm volatile("sti" ::: "memory");
    while (1) {
        {
            scoped_preemptlock lock;
            tty_driver::write("world!\n");
        }
        for (int i = 0; i < 1000000; i++)
            asm volatile("pause" ::: "memory");
    }
}

void scheduler::initialize() {
    asm volatile("cli" ::: "memory");

    current_task = task_allocator.allocate();
    current_task->pid = static_cast<pid_t>(-1);  // pid given to initial kernel thread
    current_task->stack = create_kernel_stack(reinterpret_cast<reg_t>(&main_thread), current_task->stack_pointer);

    struct task *second_task = task_allocator.allocate();
    second_task->pid = static_cast<pid_t>(-2);  // pid given to initial kernel thread
    second_task->stack = create_kernel_stack(reinterpret_cast<reg_t>(&second_thread), second_task->stack_pointer);

    current_task->add_after_self(second_task);

    memset(&global_tss, 0, sizeof(global_tss));
    global_tss.ss0 = 0x10;
    enter_task(current_task->stack_pointer);

    kpanic("enter_task has returned");
}

// called from interrupt context - so need to enable interrupts
void scheduler::timeslice_passed(interrupts::interrupt_args &resume_info) {
    if (current_task == 0) [[unlikely]]
        return;  // not initialized yet
    if (__atomic_load_n(&preempt_counter, __ATOMIC_ACQUIRE) != 0)
        return;  // preemption is locked

    // set stack pointer to point to interrupt info
    current_task->stack_pointer = reinterpret_cast<char *>(&resume_info);
    // enter the next stack pointer (round robin scheduler)
    current_task = current_task->get_next();
    enter_task(current_task->stack_pointer);
}

void scheduler::preempt_down() {
    uint32_t new_val = __atomic_sub_fetch(&preempt_counter, 1, __ATOMIC_ACQ_REL);
    kassert(new_val != (uint32_t)-1);
}

void scheduler::preempt_up() {
    __atomic_add_fetch(&preempt_counter, 1, __ATOMIC_ACQ_REL);
}
