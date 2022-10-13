#include <kernel/util.hpp>
#include <kernel/tty.hpp>

void __kassert_fail_internal(const char *assertion, const char *file, uint line, const char *function) {
    using namespace formatting;
    asm volatile("cli");
    tty_driver::write(color_pair { color::red, color::white }, "\nASSERTION FAILED: ", color_pair { color::black, color::white },
            assertion, " in file ", file, ':', line, " in function ", function);
    while (1) asm("hlt");
}

void __kpanic_internal_before(void) {
    using namespace formatting;
    asm volatile("cli");
    tty_driver::write(color_pair { color::red, color::white }, "\nKERNEL PANIC: ", color_pair { color::black, color::white });
}

void __kpanic_internal_after(const char *file, uint line, const char *function) {
    tty_driver::write(" in file ", file, ':', line, " in function ", function);
    while (1) asm("hlt");
}
