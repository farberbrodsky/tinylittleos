#include <kernel/util.hpp>
#include <kernel/tty.hpp>
#include <kernel/logging.hpp>
#include <kernel/util/lock.hpp>

void __kassert_fail_internal(const char *assertion, const char *file, uint line, const char *function) {
    using namespace formatting;
    asm volatile("cli");
    tty_driver::write(color_pair { color::red, color::white }, "\nASSERTION FAILED: ", color_pair { color::black, color::white }, assertion);
    __kpanic_internal_after(file, line, function);
}

void __kpanic_internal_before(void) {
    using namespace formatting;
    asm volatile("cli");
    tty_driver::write(color_pair { color::red, color::white }, "\nKERNEL PANIC: ", color_pair { color::black, color::white });
}

struct stack_frame {
  stack_frame *ebp;
  uint32_t eip;
};

void __kpanic_internal_after(const char *file, uint line, const char *function) {
    tty_driver::write(" in file ", file, ':', line, " in function ", function);
    // stack trace
    stack_frame *frame;
    asm volatile("movl %%ebp, %0" : "=r"(frame) :: "memory");
    uintptr_t frame_min = reinterpret_cast<uintptr_t>(frame) & (~8191ULL);
    uintptr_t frame_max = frame_min + 8192;
    while (frame != nullptr && frame_min <= reinterpret_cast<uintptr_t>(frame) && reinterpret_cast<uintptr_t>(frame) <= frame_max) {
        {
            scoped_intlock lock;
            serial_driver::write("TRACE ", formatting::hex{frame->eip}, '\n');
        }
        stack_frame *next = frame->ebp;
        if (reinterpret_cast<uintptr_t>(next) <= reinterpret_cast<uintptr_t>(frame)) {
            break;
        }
        frame = next;
    }

    while (1) asm("hlt");
}
