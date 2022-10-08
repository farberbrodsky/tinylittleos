#include <kernel/tty.hpp>

extern "C" void kmain(void) {
    tty::initialize();
    tty::write("A\n");
    kassert(13 == 37);
}
