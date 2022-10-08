#pragma once
#include <kernel/util.hpp>
#include <kernel/formatting.hpp>

namespace serial {
    void initialize();
    void put(char c);
    inline void set_color(formatting::color_pair) {}
}

using serial_driver = formatting::log_driver<serial::put, serial::set_color>;
