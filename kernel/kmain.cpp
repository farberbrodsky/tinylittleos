#include <kernel/tty.hpp>
#include <kernel/serial.hpp>
#include <kernel/logging.hpp>
#include <kernel/memory/gdt.hpp>
#include <kernel/memory/multiboot.hpp>
#include <kernel/interrupts/init.hpp>
#include <kernel/interrupts/pic.hpp>

extern "C" void kmain(multiboot_info_t *multiboot_data, uint multiboot_magic) {
    tty::initialize();
    serial::initialize();
    TINY_INFO("Early boot, initialized I/O");

    memory::read_multiboot_data(multiboot_data, multiboot_magic);
    memory::init_gdt();

    interrupts::initialize();
    interrupts::sti();
    interrupts::init_pic();

    tty_driver::write("system up!\n");
    while (1) { asm volatile("hlt"); }
}
