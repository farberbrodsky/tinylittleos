#pragma once
#include <kernel/formatting.hpp>
#include <kernel/util/string.hpp>

namespace tty {
    void initialize();
    void move_cursor(unsigned short pos);
    void put(char c);
    void set_color(formatting::color_pair set_color);
}

using tty_driver = formatting::log_driver<tty::put, tty::set_color>;
