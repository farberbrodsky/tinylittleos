#include <kernel/util.hpp>
#include <kernel/tty.hpp>

void __kassert_fail_internal(const char *assertion, const char *file, uint line, const char *function) {
    using namespace formatting;
    tty_driver::write(color_pair { color::red, color::white }, "\nASSERTION FAILED: ", color_pair { color::black, color::white },
            assertion, " in file ", file, ':', line, " in function ", function);
    while (1) asm("pause");  // TODO something better than this
}

void __kpanic_internal(const char *msg, const char *file, uint line, const char *function) {
    using namespace formatting;
    tty_driver::write(color_pair { color::red, color::white }, "\nKERNEL PANIC: ", color_pair { color::black, color::white },
            msg, " in file ", file, ':', line, " in function ", function);
    while (1) asm("pause");  // TODO something better than this
}
