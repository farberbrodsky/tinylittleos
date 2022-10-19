#include <kernel/util.hpp>
#include <kernel/devices/keyboard.hpp>
#include <kernel/tty.hpp>
#include <kernel/logging.hpp>
#include <kernel/util/intlock.hpp>

// 128 bits of is_down
static uint32_t is_down[4];  // global

char kbd_us[128] = {
    0,  27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b', '\t',
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n', 0,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',  0, '\\',
    'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, '*', 0,
    ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    '-', 0, 0, 0, '+', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

void devices::keyboard::on_scan_code(unsigned char scan_code) {
    scoped_intlock lock;

    if (scan_code & 0x80) {
        // up event
        scan_code ^= 0x80;  // turn of that bit
        is_down[scan_code >> 5] &= ~(1u << (scan_code & 0x1f));
    } else {
        // down event
        is_down[scan_code >> 5] |=  (1u << (scan_code & 0x1f));

        char us_char = kbd_us[scan_code];
        if (us_char != 0) {
            if (('a' <= us_char) && (us_char <= 'z') && (is_down[1] & 1024)) {
                // shift is down
                us_char -= 32;
            }

            tty::put(us_char);
        }
    }
}
