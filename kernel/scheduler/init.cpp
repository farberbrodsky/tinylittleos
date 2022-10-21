#include <kernel/scheduler/init.hpp>
#include <kernel/util/string.hpp>
#include <kernel/memory/page_allocator.hpp>
#include <kernel/util/asm_wrap.hpp>

scheduler::tss_entry scheduler::global_tss;
static uint32_t preempt_counter = 0;  // global

static void sleep() {
    quality_debugging();
    asm volatile("int3");
}

void scheduler::initialize() {
    memset(&global_tss, 0, sizeof(global_tss));
    // allocate interrupt stack page
    global_tss.esp0 = (uint32_t)memory::kmem_alloc_8k();
    global_tss.ss0 = 0x10;

    asm_enter_usermode((void *)&sleep, memory::kmem_alloc_8k());
}

void scheduler::preempt_down() {
    uint32_t new_val = __atomic_sub_fetch(&preempt_counter, 1, __ATOMIC_ACQ_REL);
    kassert(new_val != (uint32_t)-1);
}

void scheduler::preempt_up() {
    __atomic_add_fetch(&preempt_counter, 1, __ATOMIC_ACQ_REL);
}
