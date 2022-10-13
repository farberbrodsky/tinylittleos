#include <kernel/serial.hpp>
#include <kernel/util/asm_wrap.hpp>

// The I/O ports
static constexpr unsigned short SERIAL_COM1_BASE = 0x3F8;  // COM1 base port
static inline constexpr unsigned short SERIAL_DATA_PORT(unsigned short base)          { return base; }
static inline constexpr unsigned short SERIAL_FIFO_COMMAND_PORT(unsigned short base)  { return base + 2; }
static inline constexpr unsigned short SERIAL_LINE_COMMAND_PORT(unsigned short base)  { return base + 3; }
static inline constexpr unsigned short SERIAL_MODEM_COMMAND_PORT(unsigned short base) { return base + 4; }
static inline constexpr unsigned short SERIAL_LINE_STATUS_PORT(unsigned short base)   { return base + 5; }

// The I/O port commands

/* SERIAL_LINE_ENABLE_DLAB:
 * Tells the serial port to expect first the highest 8 bits on the data port,
 * then the lowest 8 bits will follow
 */
static constexpr unsigned char SERIAL_LINE_ENABLE_DLAB = 0x80;

/** serial_configure_baud_rate:
 *  Sets the speed of the data being sent. The default speed of a serial
 *  port is 115200 bits/s. The argument is a divisor of that number, hence
 *  the resulting speed becomes (115200 / divisor) bits/s.
 *
 *  @param com      The COM port to configure
 *  @param divisor  The divisor
 */
static void serial_configure_baud_rate(unsigned short com, unsigned short divisor) {
    asm_outb(SERIAL_LINE_COMMAND_PORT(com),
         SERIAL_LINE_ENABLE_DLAB);
    asm_outb(SERIAL_DATA_PORT(com),
         (divisor >> 8) & 0x00FF);
    asm_outb(SERIAL_DATA_PORT(com),
         divisor & 0x00FF);
}


/** serial_configure_line:
 *  Configures the line of the given serial port. The port is set to have a
 *  data length of 8 bits, no parity bits, one stop bit and break control
 *  disabled.
 *
 *  @param com  The serial port to configure
 */
static void serial_configure_line(unsigned short com) {
    /* Bit:     | 7 | 6 | 5 4 3 | 2 | 1 0 |
     * Content: | d | b | prty  | s | dl  |
     * Value:   | 0 | 0 | 0 0 0 | 0 | 1 1 | = 0x03
     */
    asm_outb(SERIAL_LINE_COMMAND_PORT(com), 0x03);
}

static bool serial_is_transmit_fifo_empty(unsigned short com) {
    return (asm_inb(SERIAL_LINE_STATUS_PORT(com)) & 0x20) != 0;
}

void serial::initialize() {
    serial_configure_baud_rate(SERIAL_COM1_BASE, 1);
    serial_configure_line(SERIAL_COM1_BASE);
}

void serial::put(char c) {
    while (!serial_is_transmit_fifo_empty(SERIAL_COM1_BASE))
        asm("pause");

    asm_outb(SERIAL_DATA_PORT(SERIAL_COM1_BASE), c);
}
