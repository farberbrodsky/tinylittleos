#include <kernel/scheduler/init.hpp>
#include <kernel/scheduler/task.hpp>
#include <kernel/util/string.hpp>
#include <kernel/memory/page_allocator.hpp>
#include <kernel/util/asm_wrap.hpp>
#include <kernel/memory/slab.hpp>
#include <kernel/memory/gdt.hpp>
#include <kernel/logging.hpp>
#include <kernel/interrupts/init.hpp>

scheduler::tss_entry scheduler::global_tss;
static volatile uint32_t preempt_counter = 0;  // global
static memory::slab_allocator<scheduler::task> task_allocator;

scheduler::task *volatile scheduler::current_task = 0;

[[noreturn]] static void enter_task(char *stack) {
    scheduler::global_tss.esp0 = reinterpret_cast<reg_t>(stack) + sizeof(interrupts::interrupt_args);
    asm_enter_task(stack);
}

char stack_free_stack[4096];  // stack used to free the stack

extern "C" __attribute__((cdecl)) void _sched_final_free(char *free_stack, char *switch_stack) {
    memory::kmem_free_4k(free_stack);
    scheduler::preempt_down();  // worst case scenario: this task is already freed at the point of switch
    enter_task(switch_stack);
    __builtin_unreachable();
}

static void task_wrapper(void (*call)(void)) {
    using namespace scheduler;
    asm volatile("sti" ::: "memory");
    // initialization

    // initialize hmem mappings - top page table
    void *hmem_mappings_page_table = memory::kmem_alloc_4k();
    memset(hmem_mappings_page_table, 0, 4096);
    uint32_t cr3;
    asm volatile("movl %%cr3, %0" : "=r"(cr3) :: "memory");
    uint32_t *page_dir = reinterpret_cast<uint32_t *>(memory::phys_t(cr3).to_virt());
    page_dir[0x3FF] = memory::phys_t::from_kmem(hmem_mappings_page_table).value() |
        static_cast<uint32_t>(memory::page_flag::present) | static_cast<uint32_t>(memory::page_flag::write);

    call();

    // finalization
    preempt_up();  // during finalization, disable preemption
    task *me = current_task;
    current_task = task::from(me->scheduling.get_next());
    unlink_task(me);
    task::release(me);
    memory::kmem_free_4k(hmem_mappings_page_table);

    // call _sched_final_free from another stack (stack_free_stack)
    char *my_stack = reinterpret_cast<char *>(get_current_task_internal());
    *reinterpret_cast<reg_t *>(stack_free_stack + sizeof(stack_free_stack) -     sizeof(reg_t)) = reinterpret_cast<reg_t>(current_task->stack_pointer);
    *reinterpret_cast<reg_t *>(stack_free_stack + sizeof(stack_free_stack) - 2 * sizeof(reg_t)) = reinterpret_cast<reg_t>(my_stack);

    char *new_sp = stack_free_stack + sizeof(stack_free_stack) - 2 * sizeof(reg_t);
    asm volatile("":::"memory");
    asm volatile("mov %0, %%esp \n\t cld \n\t call _sched_final_free" :: "g" (new_sp) : "memory");
    __builtin_unreachable();
}

// Creates a task which can be entered to run task_wrapper with main as the argument
static char *create_kernel_stack(void (*main)(void), reg_t cr3) {
    // TODO use hmem instead of kmem for stacks
    char *stack = static_cast<char *>(memory::kmem_alloc_8k());
    ((scheduler::task_internal *)stack)->hmem_end = 0;

    stack = stack + 8192 - sizeof(interrupts::interrupt_args) - sizeof(reg_t) - sizeof(reg_t);  // Do NOT change without changing get_current_task_internal
    interrupts::interrupt_args *info = reinterpret_cast<interrupts::interrupt_args *>(stack);
    memset(info, 0, sizeof(interrupts::interrupt_args));
    info->cr3 = cr3;
    info->eip = reinterpret_cast<reg_t>(task_wrapper);
    reg_t &arg = *reinterpret_cast<reg_t *>(stack + sizeof(interrupts::interrupt_args) + sizeof(reg_t));
    arg = reinterpret_cast<reg_t>(main);
    info->cs = memory::kernel_cs;
    // EFLAGS is 0 so interrupts are initially disabled
    return stack;
}

scheduler::task::task(uint32_t _pid, char *_stack_pointer) : pid(_pid), stack_pointer(_stack_pointer)
{}

static pid_t max_pid;
scheduler::task *scheduler::task::allocate(void (*run)(void)) {
    kassert_not_interrupt;
    scoped_preemptlock lock;
    pid_t pid = max_pid++;
    kassert(pid < 16384);  // implement it better! allow reuse of pids
    task *t = task_allocator.allocate(pid, create_kernel_stack(run, memory::new_page_directory()));
    return t;
}

void scheduler::task::release(task *obj) {
    if (obj->release_ref()) {
        task_allocator.free(obj);
    }
}

static void idle_task() {
    while (1) {
        asm volatile("pause" ::: "memory");
    }
}

void scheduler::initialize() {
    asm volatile("cli" ::: "memory");
    max_pid = 0;

    preempt_counter = 1;  // do not switch task yet
    current_task = task_allocator.allocate(max_pid++, create_kernel_stack(idle_task, memory::new_page_directory()));
    // TODO make idle thread lower priority so it only runs if it has to

    memset(&global_tss, 0, sizeof(global_tss));
    global_tss.ss0 = 0x10;
}

void scheduler::start() {
    scoped_intlock lock;  // disable interrupts before enter_task
    preempt_counter = 0;  // can switch tasks from now on
    enter_task(current_task->stack_pointer);
    kpanic("enter_task has returned");
}

void scheduler::link_task(task *t) {
    t->take_ref();
    current_task->scheduling.add_after_self(&t->scheduling);
}

void scheduler::unlink_task(task *t) {
    t->scheduling.unlink();
    t->release_ref();
}

void scheduler::pick_next_task()
{
    do {
        current_task = task::from(current_task->scheduling.get_next());
    } while (current_task->blocking.is_blocked());
}

// called from interrupt context - so need to enable interrupts
void scheduler::timeslice_passed(interrupts::interrupt_args &resume_info) {
    kassert_is_interrupt;
    // no interrupts in this function
    scoped_intlock lock;

    if (current_task == 0) [[unlikely]]
        return;  // not initialized yet
    if (__atomic_load_n(&preempt_counter, __ATOMIC_ACQUIRE) != 0 || interrupts::get_interrupt_context_depth() > 1)
        return;  // preemption is locked

    interrupts::reduce_interrupt_depth();
    // set stack pointer to point to interrupt info
    current_task->stack_pointer = reinterpret_cast<char *>(&resume_info);
    // enter the next stack pointer
    pick_next_task();
    enter_task(current_task->stack_pointer);
}

void scheduler::preempt_down() {
    uint32_t new_val = __atomic_sub_fetch(&preempt_counter, 1, __ATOMIC_ACQ_REL);
    kassert(new_val != (uint32_t)-1);
}

void scheduler::preempt_up() {
    __atomic_add_fetch(&preempt_counter, 1, __ATOMIC_ACQ_REL);
}

extern "C" __attribute__((cdecl)) void do_yield(interrupts::interrupt_args *stack_arg) {
    using namespace scheduler;

    // set stack pointer to point to interrupt info
    current_task->stack_pointer = reinterpret_cast<char *>(stack_arg);
    // enter the next stack pointer
    pick_next_task();
    enter_task(current_task->stack_pointer);
}

void scheduler::yield() {
    kassert_not_interrupt;
    kassert(preempt_counter == 0);
    // disable interrupts while working on switching
    interrupts::cli();

    asm volatile(
            ""
            "pushf                  \n"  // eflags
            "pushl %0               \n"  // cs
            "pushl $0x00            \n"  // placeholder for eip
            "pushl $0x00            \n"  // error_code
            "pushl $0x00            \n"  // interrupt_number
            "pushl %%eax            \n"  // eax
            "call .get_eip          \n"  // find the eip now
            "0:                     \n"  // jump back here later
            "pushl %%ebx            \n"  // ebx
            "pushl %%ecx            \n"  // ecx
            "pushl %%edx            \n"  // edx
            "pushl %%esi            \n"  // esi
            "pushl %%edi            \n"  // edi
            "pushl %%ebp            \n"  // ebp
            "movl %%cr3, %%eax      \n"  // cr3
            "pushl %%eax            \n"  // ...
            "pushl %%esp            \n"  // give this whole stack as a paremeter to do_yield
            "cld                    \n"  // call do_yield with this parameter
            "call do_yield          \n"  // ...
            ".get_eip:              \n"  // find the eip of 1: and then jump back
            "popl %%eax             \n"  // ...
            "addl $1f - 0b, %%eax   \n"  // ...
            "mov %%eax, +12(%%esp)  \n"  // ... we have pushed 12 other bytes (error, interrupt, eax)
            "jmp 0b                 \n"  // ...
            "1:                     \n"  // the eip pointed to in the stack
            "" : /* output */ : /* input */ "n"(memory::kernel_cs) : /* clobber */ "memory");
}
