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
#include <kernel/scheduler/elf.hpp>
#include <kernel/util/asm_wrap.hpp>

static void show_splash();

static void main_task() {
    fs::inode *a;
    fs::file_desc *f;
    errno e = fs::traverse("/initrd/shell.elf", a);
    kassert(e == errno::ok);
    e = a->open(f);
    kassert(e == errno::ok);
    void *entry;
    e = elf_loader::load_elf(f, entry);
    kassert(e == errno::ok);
    asm_enter_usermode(entry, memory::kmem_alloc_4k());
}

static void second_task() {
    while (1) {
        {
            scoped_preemptlock lock;
            tty_driver::write("hello world!\n");
        }
        for (int i = 0; i < 1000000; i++)
            asm volatile("pause" ::: "memory");
    }
}

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

    scheduler::initialize();
    scheduler::task *main = scheduler::task::allocate(main_task);
    scheduler::task *second = scheduler::task::allocate(second_task);
    kassert(main != nullptr && second != nullptr);
    scheduler::link_task(main);
    scheduler::link_task(second);
    // do not release tasks, because the task itself wants to have a reference
    scheduler::start();
    kpanic("scheduler::start returned");
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
