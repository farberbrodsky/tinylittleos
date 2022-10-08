#include <kernel/tty.hpp>
#include <kernel/serial.hpp>

extern "C" void kmain(void) {
    tty::initialize();
    serial::initialize();
    tty_driver::write("Hello!\n");
    serial_driver::write("[LOG] ", "hello world\n");
    serial_driver::write("[LOG] ", "tihs works!!!\n");
    kassert(13 == 37);
    while (1) { asm("hlt"); }
}
