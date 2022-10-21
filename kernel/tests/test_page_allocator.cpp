#include <kernel/tty.hpp>
#include <kernel/serial.hpp>
#include <kernel/logging.hpp>
#include <kernel/memory/multiboot.hpp>
#include <kernel/memory/gdt.hpp>
#include <kernel/memory/page_allocator.hpp>
#include <kernel/memory/slab.hpp>
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
    kmem_free_4k((void *)p3);
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

static void test_slab() {
    using namespace memory;
    struct __attribute__((packed)) weird {
        volatile char a[17];
        weird(int) {
            a[0] = 3;
        }
        weird(int, int) {
            a[0] = 4;
        }
        ~weird() {
            a[0] = 5;
        }
    };
    static_assert(sizeof(weird) == 17, "packed struct");
    static_assert(__slab_allocator<2049>::page_size == 32768, "slab allocator page size selection");
    static_assert(__slab_allocator<2000>::page_size == 4096, "slab allocator page size selection");
    static_assert(__slab_allocator<17>::page_size == 4096, "slab allocator page size selection");

    slab_allocator<weird> alloc;
    weird *a = alloc.allocate(2222);
    weird *b = alloc.allocate(13, 37);
    kassert((((uint32_t)a) + 17) == (uint32_t)b);
    kassert((a->a[0] == 3) && (b->a[0] == 4));
    alloc.free(a);
    kassert((a->a[0] == 5) && (b->a[0] == 4));
    alloc.free(b);
    kassert((a->a[0] == 5) && (b->a[0] == 5));

    weird **arr = (weird **)kmem_alloc_4k();
    for (int i = 0; i < 410; i++) {
        if (i < 400) arr[i] = alloc.allocate(1);
        if (i >= 10) alloc.free(arr[i - 10]);
    }
    TINY_INFO("Pass test slab");
}

extern "C" void kmain(multiboot_info_t *multiboot_data, uint multiboot_magic) {
    serial::initialize();

    memory::read_multiboot_data(multiboot_data, multiboot_magic);
    memory::init_gdt();
    memory::init_page_allocator();

    interrupts::initialize();
    interrupts::init_pic();
    interrupts::start();

    test_0();
    test_big();
    test_slab();

    // test done
    interrupts::cli();
    serial_driver::write("TEST_SUCCESS");
    while (1) { asm volatile("hlt"); }
}
