#include <kernel/tty.hpp>
#include <kernel/serial.hpp>
#include <kernel/logging.hpp>
#include <kernel/memory/gdt.hpp>
#include <kernel/interrupts.hpp>

extern "C" void kmain(void) {
    tty::initialize();
    serial::initialize();
    TINY_INFO("Early boot, initialized I/O");
    memory::init_gdt();
    interrupts::initialize();
    tty_driver::write("Hello!\n");
    asm volatile("int3");
    asm volatile("int3");
    kassert(13 == 37);
    while (1) { asm("hlt"); }
}
