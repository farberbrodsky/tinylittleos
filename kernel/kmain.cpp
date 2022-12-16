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
#include <kernel/fs/vfs.hpp>

static void show_splash();

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

    fs::register_initrd("/initrd");
    show_splash();

    while (1) { asm volatile("hlt"); }
}

static void show_splash() {
    fs::inode *a;
    fs::file_desc *f;
    errno e = fs::traverse("/initrd/splash.txt", a);
    kassert(e == errno::ok);
    e = a->open(f);
    kassert(e == errno::ok);
    char *buf = static_cast<char *>(memory::kmem_alloc_4k());
    ssize_t read_len = f->read(buf, 4096);
    kassert(read_len <= 4096 && read_len >= 0);
    tty_driver::write(string_buf{buf, static_cast<size_t>(read_len)});
    f->release(f);
    a->release(a);
}
