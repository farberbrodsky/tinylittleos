#pragma once
#include <kernel/util.hpp>
#include <kernel/util/string.hpp>

namespace tty {
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

    enum class mode {
        null = 0,
        hex = 1
    };

    struct hex {
        uintptr_t val;
        inline explicit hex(uintptr_t val) : val{val} {}
        inline explicit hex(void *val) : val{(uintptr_t)val} {}
    };

    void initialize();

    void move_cursor(unsigned short pos);

    void write(char c);
    void write(int val);
    void write(uint val);
    void write(hex val);
    void write(color_pair set_color);

    inline void write(const string_buf &s) {
        for (size_t i = 0; i < s.length; i++) {
            write(s.data[i]);
        }
    }

    // variadic version
    inline void __write_rec() {}
    template <class T, class... Args>
    inline void __write_rec(T t, Args... args) {
        tty::write(t);
        tty::__write_rec(args...);
    }
    template <class... Args>
    inline void write(Args... args) {
        tty::__write_rec(args...);
    }
}
