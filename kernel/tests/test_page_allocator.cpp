#include <kernel/tty.hpp>
#include <kernel/serial.hpp>
#include <kernel/logging.hpp>
#include <kernel/memory/multiboot.hpp>
#include <kernel/memory/gdt.hpp>
#include <kernel/memory/page_allocator.hpp>
#include <kernel/interrupts/init.hpp>
#include <kernel/interrupts/pic.hpp>

static void test_0() {
    using namespace memory;
    uint32_t p1 = (uint32_t)kmem_alloc_4k();
    uint32_t p2 = (uint32_t)kmem_alloc_8k();
    kassert(p2 == (p1 + 4096));
    kmem_free_4k((void *)p1);
    uint32_t p3 = (uint32_t)kmem_alloc_4k();
    kassert(p1 == p3);
    kmem_free_8k((void *)p2);
    kmem_free_8k((void *)p3);
    TINY_INFO("Pass test 0");
}

// check that we can allocate a lot of memory
static void test_big() {
    using namespace memory;
    void **arr = (void **)kmem_alloc_4k();

    for (int i = 0; i < 500; i += 4) {
        arr[i] = kmem_alloc_4k();
        arr[i + 1] = kmem_alloc_32k();
        arr[i + 2] = kmem_alloc_8k();
        arr[i + 3] = kmem_alloc_4k();
        kmem_free_32k(arr[i + 1]);
    }

    for (int i = 200; i < 700; i += 4) {
        int j = i;
        if (j >= 500) j -= 500;
        kmem_free_4k(arr[j]);
        kmem_free_8k(arr[j + 2]);
        kmem_free_4k(arr[j + 3]);
    }

    kmem_free_4k((void *)arr);
    TINY_INFO("Pass test big");
}

extern "C" void kmain(multiboot_info_t *multiboot_data, uint multiboot_magic) {
    serial::initialize();

    memory::read_multiboot_data(multiboot_data, multiboot_magic);
    memory::init_gdt();
    memory::init_page_allocator();

    interrupts::initialize();
    interrupts::sti();

    test_0();
    test_big();

    // test done
    interrupts::cli();
    serial_driver::write("TEST_SUCCESS");
    while (1) { asm volatile("hlt"); }
}
