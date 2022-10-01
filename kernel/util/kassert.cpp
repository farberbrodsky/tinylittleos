#include <kernel/util.hpp>
#include <kernel/tty.hpp>

void __kassert_fail_internal(const char *assertion, const char *file, uint line, const char *function) {
    using namespace tty;
    int_to_str_stack_buf stack_buf;
    string_buf buf = str_util::dec(line, stack_buf);
    tty::write(color_pair { color::red, color::white }, "\nASSERTION FAILED: ", color_pair { color::black, color::white },
            assertion, " in file ", file, ':', buf, " in function ", function);
    while (1) asm("pause");  // TODO something better than this
}

void __kpanic_internal(const char *msg, const char *file, uint line, const char *function) {
    using namespace tty;
    int_to_str_stack_buf stack_buf;
    string_buf buf = str_util::dec(line, stack_buf);
    tty::write(color_pair { color::red, color::white }, "\nKERNEL PANIC: ", color_pair { color::black, color::white },
            msg, " in file ", file, ':', buf, " in function ", function);
    while (1) asm("pause");  // TODO something better than this
}
