#include <kernel/tty.hpp>
#include <kernel/serial.hpp>
#include <kernel/logging.hpp>
#include <kernel/memory/multiboot.hpp>
#include <kernel/memory/gdt.hpp>
#include <kernel/memory/page_allocator.hpp>
#include <kernel/interrupts/init.hpp>
#include <kernel/interrupts/pic.hpp>
#include <kernel/util/ds/bitset.hpp>
#include <kernel/util/ds/list.hpp>
#include <kernel/util/ds/hashtable.hpp>
#include <kernel/util/ds/refcount.hpp>

static void test_bitset() {
    // create bitset on non-zeroed memory
    char memory[12];
    memset(memory, 0xFF, sizeof(memory));
    ds::bitset<70> &bits = *(new (&memory) ds::bitset<70>());
    static_assert(sizeof(ds::bitset<70>) == 12);

    for (int i = 0; i < 70; i++) {
        kassert(bits.has_bit(i) == false);
        if ((i % 3) == 0) bits.set_bit(i);
    }
    for (int i = 0; i < 70; i += 3) {
        kassert(bits.has_bit(i) == true);
        bits.clear_bit(i);
    }

    ds::bitset<70> bits2;
    bits2.set_bit(50);
    ds::bitset<70> bits3(bits2);
    kassert(bits2.has_bit(50));
    kassert(bits3.has_bit(50));
    bits2 = bits;
    kassert(!bits2.has_bit(50));
    kassert(bits3.has_bit(50));
    TINY_INFO("Pass test_bitset");
}

static void test_linked_list() {
    struct A : ds::intrusive_doubly_linked_node<A> {
        int m_x;
        inline A(int x) : m_x(x) {};
    };

    A a(7);
    A b(4);
    A c(2);
    A d(3);
    A e(1);
    a.add_after_self(&b);  // a, b
    a.add_after_self(&c);  // a, c, b
    c.add_after_self(&d);  // a, c, d, b
    b.add_after_self(&e);  // a, c, d, b, e
    a.unlink();            // e, c, d, b     (remember: circular)
    int i = 1;
    for (A &it : e) {
        kassert(it.m_x == (i++));
    }
    kassert(i == 5);
    // check that unlinked item is a singular list of itself
    for (A &it : a) {
        i = 1;
        kassert(it.m_x == 7);
    }
    kassert(i == 1);

    TINY_INFO("Pass test_linked_list");
}

static void test_hash_table() {
    ds::hashtable<2> ht;

    kassert(ht.lookup(1337) == nullptr);
    ht.insert(123, (void *)456);
    ht.insert(22, (void *)22);
    ht.insert(13, (void *)37);
    kassert(ht.lookup(13) == (void *)37);
    ht.remove(22);
    kassert(ht.lookup(22) == nullptr);
    kassert(ht.lookup(123) == (void *)456);

    TINY_INFO("Pass test_hash_table");
}

static void test_refcount() {
    struct A : ds::intrusive_refcount {};
    A a {};  // starts with 1 ref
    a.take_ref();
    a.take_ref();
    kassert(a.release_ref() == false);
    a.take_ref();
    kassert(a.release_ref() == false);
    kassert(a.release_ref() == false);
    kassert(a.release_ref() == true);

    TINY_INFO("Pass test_refcount");
}

extern "C" void kmain(multiboot_info_t *multiboot_data, uint multiboot_magic) {
    serial::initialize();

    memory::read_multiboot_data(multiboot_data, multiboot_magic);
    memory::init_gdt();
    memory::init_page_allocator();

    interrupts::initialize();
    interrupts::init_pic();
    interrupts::start();

    test_bitset();
    test_linked_list();
    test_hash_table();
    test_refcount();

    // test done
    interrupts::cli();
    serial_driver::write("TEST_SUCCESS");
    while (1) { asm volatile("hlt"); }
}
