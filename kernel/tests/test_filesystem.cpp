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

    fs::register_initrd("/");
    fs::inode *a;
    fs::file_desc *f;
    char buf[1024];

    errno e = fs::traverse("/hello.txt", a);
    kassert(e == errno::ok);
    serial_driver::write("hello.txt ", a->i_num, '\n');
    e = a->open(f);
    kassert(e == errno::ok);
    kassert(f->get_rc_for_test() == 1);
    kassert(a->get_rc_for_test() == 2);
    ssize_t read_len = f->read(buf, 1024);
    kassert(read_len > 0);
    fs::file_desc::release(f);
    kassert(a->get_rc_for_test() == 1);
    TINY_INFO("contents: ", buf);
    fs::inode::release(a);

    e = fs::traverse("/world.txt", a);
    kassert(e == errno::ok);
    serial_driver::write("world.txt ", a->i_num, '\n');
    fs::inode::release(a);

    e = fs::traverse("/foo/bar.txt", a);
    kassert(e == errno::ok);
    serial_driver::write("/foo/bar.txt ", a->i_num, '\n');
    fs::inode::release(a);

    e = fs::traverse("/foo", a);
    kassert(e == errno::ok);
    serial_driver::write("/foo ", a->i_num, '\n');
    fs::inode::release(a);

    e = fs::traverse("/does/not/exist", a);
    kassert(e == errno::no_entry);

    // test done
    interrupts::cli();
    serial_driver::write("TEST_SUCCESS");
    while (1) { asm volatile("hlt"); }
}
