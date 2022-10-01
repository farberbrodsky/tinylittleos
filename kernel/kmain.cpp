#include <kernel/tty.hpp>

extern "C" void kmain(void) {
    tty::initialize();
    while (1) {
        tty::write("A\n");
        kassert(13 == 37);
        tty::write("B\n");
        quality_debugging();
    }
}
