#pragma once
#include <kernel/util.hpp>
#include <kernel/util/string.hpp>

// TODO reentrancy, logs should not be intertwined
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

        static void _write(errno e) {
            switch (e) {
                case errno::ok:
                    _write("errno::ok");
                    break;
                case errno::not_permitted:
                    _write("errno::not_permitted");
                    break;
                case errno::no_entry:
                    _write("errno::no_entry");
                    break;
                case errno::no_process:
                    _write("errno::no_process");
                    break;
                case errno::interrupted:
                    _write("errno::interrupted");
                    break;
                case errno::io_error:
                    _write("errno::io_error");
                    break;
                case errno::no_memory:
                    _write("errno::no_memory");
                    break;
                case errno::no_access:
                    _write("errno::no_access");
                    break;
                case errno::exists:
                    _write("errno::exists");
                    break;
                case errno::not_dir:
                    _write("errno::not_dir");
                    break;
                case errno::is_dir:
                    _write("errno::is_dir");
                    break;
                case errno::path_too_long:
                    _write("errno::path_too_long");
                    break;
            }
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
