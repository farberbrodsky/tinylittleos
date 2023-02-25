#pragma once
#include <kernel/serial.hpp>

#define LOCKED(expr) do { \
    scoped_intlock lock; \
    expr; \
} while (0);
#define TINY_INFO(expr...) LOCKED(serial_driver::write("[INFO] ", expr, " in file ", __FILE__, ':', __LINE__, ' ', '(', __PRETTY_FUNCTION__, ')', '\n'))
#define TINY_WARN(expr...) LOCKED(serial_driver::write("[WARN] ", expr, " in file ", __FILE__, ':', __LINE__, ' ', '(', __PRETTY_FUNCTION__, ')', '\n'))
#define TINY_ERR(expr...)  LOCKED(serial_driver::write("[ERR!] ", expr, " in file ", __FILE__, ':', __LINE__, ' ', '(', __PRETTY_FUNCTION__, ')', '\n'))
