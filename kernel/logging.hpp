#pragma once
#include <kernel/serial.hpp>

#define TINY_INFO(expr...) serial_driver::write("[INFO] ", expr, " in file ", __FILE__, ':', __LINE__, ' ', '(', __PRETTY_FUNCTION__, ')', '\n');
#define TINY_WARN(expr...) serial_driver::write("[WARN] ", expr, " in file ", __FILE__, ':', __LINE__, ' ', '(', __PRETTY_FUNCTION__, ')', '\n');
#define TINY_ERR(expr...)  serial_driver::write("[ERR!] ", expr, " in file ", __FILE__, ':', __LINE__, ' ', '(', __PRETTY_FUNCTION__, ')', '\n');
