#include <kernel/tty.hpp>
#include <kernel/serial.hpp>
#include <kernel/logging.hpp>
#include <kernel/memory/multiboot.hpp>
#include <kernel/memory/gdt.hpp>
#include <kernel/memory/page_allocator.hpp>
#include <kernel/interrupts/init.hpp>
#include <kernel/interrupts/pic.hpp>
#include <kernel/scheduler/init.hpp>
#include <kernel/fs/tar.hpp>

extern "C" void kmain(multiboot_info_t *multiboot_data, uint multiboot_magic) {
    tty::initialize();
    serial::initialize();
    TINY_INFO("Early boot, initialized VGA and serial");

    memory::read_multiboot_data(multiboot_data, multiboot_magic);
    memory::init_gdt();
    memory::init_page_allocator();

    interrupts::initialize();
    interrupts::init_pic();
    interrupts::start();

    fs::register_tar();
    fs::inode *a;

    errno e = fs::traverse("/hello.txt", a);
    kassert(e == errno::ok);
    serial_driver::write("hello.txt ", a->i_num, '\n');
    release_inode_struct(a);

    e = fs::traverse("/world.txt", a);
    kassert(e == errno::ok);
    serial_driver::write("world.txt ", a->i_num, '\n');
    release_inode_struct(a);

    e = fs::traverse("/foo/bar.txt", a);
    kassert(e == errno::ok);
    serial_driver::write("/foo/bar.txt ", a->i_num, '\n');
    release_inode_struct(a);

    e = fs::traverse("/foo", a);
    kassert(e == errno::ok);
    serial_driver::write("/foo ", a->i_num, '\n');
    release_inode_struct(a);

    e = fs::traverse("/does/not/exist", a);
    kassert(e == errno::no_entry);

    // test done
    interrupts::cli();
    serial_driver::write("TEST_SUCCESS");
    while (1) { asm volatile("hlt"); }
}
