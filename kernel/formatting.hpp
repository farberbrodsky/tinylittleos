#pragma once
#include <kernel/util.hpp>
#include <kernel/util/string.hpp>

namespace formatting {
    struct hex {
        uintptr_t val;
        inline explicit hex(uintptr_t val) : val{val} {}
        inline explicit hex(void *val) : val{(uintptr_t)val} {}
    };

    enum class color {
        black = 0,
        blue = 1,
        green = 2,
        cyan = 3,
        red = 4,
        magenta = 5,
        brown = 6,
        light_gray = 7,
        dark_gray = 8,
        light_blue = 9,
        light_green = 10,
        light_cyan = 11,
        light_red = 12,
        light_magenta = 13,
        light_brown = 14,
        white = 15
    };

    struct color_pair {
        color fg;
        color bg;
        inline explicit color_pair(color fg, color bg) : fg{fg}, bg{bg} {}
    };

    template <void F(char c), void G(color_pair set_color)>
    struct log_driver {
    private:
        static inline void _write(char c) {
            F(c);
        }

        static inline void _write(const string_buf &s) {
            for (size_t i = 0; i < s.length; i++) {
                write(s.data[i]);
            }
        }

        static inline void _write(int val) {
            int_to_str_stack_buf buf;
            write(str_util::dec(val, buf));
        }

        static inline void _write(uint val) {
            int_to_str_stack_buf buf;
            write(str_util::dec(val, buf));
        }

        static inline void _write(hex val) {
            int_to_str_stack_buf buf;
            write(str_util::hex(val.val, buf));
        }

        static inline void _write(color_pair set_color) {
            G(set_color);
        }

    public:
        // variadic version
        static inline void write() {}

        template <class T, class... Args>
        static inline void write(T t, Args... args) {
            _write(t);
            write(args...);
        }
    };
}
