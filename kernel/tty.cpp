#include <kernel/util.hpp>
#include <kernel/tty.hpp>
#include <kernel/util/asm_wrap.hpp>
#include <kernel/serial.hpp>

using formatting::color;
using formatting::color_pair;

static constexpr size_t VGA_WIDTH = 80;
static constexpr size_t VGA_HEIGHT = 25;
static constexpr uintptr_t VGA_MEMORY = 0xC00B8000;

static size_t t_x;
static size_t t_y;
static uint8_t t_color;
static volatile uint16_t *t_buf;
static uint16_t internal_buf[VGA_WIDTH * VGA_HEIGHT];
size_t internal_buf_i;  // ring buffer

static inline uint8_t vga_color(color fg, color bg) {
    return (uint8_t)fg | ((uint8_t)bg << 4);
}

static inline uint16_t vga_entry(unsigned char code, uint8_t color) {
    return (uint16_t)code | (uint16_t)color << 8;
}

static inline void set_char(unsigned char code, uint8_t color, size_t x, size_t y) {
    t_buf[y * VGA_WIDTH + x] = vga_entry(code, color);
    internal_buf[((y + internal_buf_i) % VGA_HEIGHT) * VGA_WIDTH + x] = vga_entry(code, color);
}

void tty::initialize() {
    t_x = 0;
    t_y = 0;
    internal_buf_i = 0;
    set_color(color_pair {color::black, color::white});
    t_buf = reinterpret_cast<volatile uint16_t *>(VGA_MEMORY);
	for (size_t y = 0; y < VGA_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
            set_char(' ', t_color, x, y);
		}
	}
}


static constexpr unsigned short FB_COMMAND_PORT = 0x3D4;
static constexpr unsigned short FB_DATA_PORT    = 0x3D5;
static constexpr unsigned char  FB_HIGH_BYTE_COMMAND = 14;
static constexpr unsigned char  FB_LOW_BYTE_COMMAND  = 15;

void tty::move_cursor(unsigned short pos) {
#ifndef TINY_TEST
    asm_outb(FB_COMMAND_PORT, FB_HIGH_BYTE_COMMAND);
    asm_outb(FB_DATA_PORT,    ((pos >> 8) & 0x00FF));
    asm_outb(FB_COMMAND_PORT, FB_LOW_BYTE_COMMAND);
    asm_outb(FB_DATA_PORT,    pos & 0x00FF);
#else
    kunused(pos);
#endif
}

#ifndef TINY_TEST
static void tty_scroll() {
    if (++internal_buf_i == VGA_HEIGHT) internal_buf_i = 0;
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            t_buf[y * VGA_WIDTH + x] = internal_buf[((y + internal_buf_i) % VGA_HEIGHT) * VGA_WIDTH + x];
        }
    }
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        set_char(' ', t_color, x, VGA_HEIGHT - 1);
    }
}
#endif

#ifdef TINY_TEST
void tty::put(char c) {
    serial::put(c);
}
#else
void tty::put(char c) {
    if (c == '\n') {
        t_x = 0;
        if (t_y == VGA_HEIGHT - 1) {
            tty_scroll();
        } else {
            t_y++;
        }
    } else {
        unsigned char uc = c;
        set_char(uc, t_color, t_x++, t_y);
        if (t_x == VGA_WIDTH) {
            t_x = 0;
            if (t_y == VGA_HEIGHT - 1) {
                tty_scroll();
            } else {
                t_y++;
            }
        }
    }

    tty::move_cursor(t_x + t_y * VGA_WIDTH);
}
#endif

#ifdef TINY_TEST
void tty::set_color(color_pair set_color) {
    serial::set_color(set_color);
}
#else
void tty::set_color(color_pair set_color) {
    t_color = vga_color(set_color.fg, set_color.bg);
}
#endif
