#include <kernel/tty.hpp>
#include <kernel/serial.hpp>
#include <kernel/logging.hpp>
#include <kernel/memory/multiboot.hpp>
#include <kernel/memory/gdt.hpp>
#include <kernel/memory/page_allocator.hpp>
#include <kernel/interrupts/init.hpp>
#include <kernel/interrupts/pic.hpp>
#include <kernel/util/ds/bitset.hpp>

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

extern "C" void kmain(multiboot_info_t *multiboot_data, uint multiboot_magic) {
    serial::initialize();

    memory::read_multiboot_data(multiboot_data, multiboot_magic);
    memory::init_gdt();
    memory::init_page_allocator();

    interrupts::initialize();
    interrupts::init_pic();
    interrupts::start();

    test_bitset();

    // test done
    interrupts::cli();
    serial_driver::write("TEST_SUCCESS");
    while (1) { asm volatile("hlt"); }
}
