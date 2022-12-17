#include <kernel/tty.hpp>
#include <kernel/logging.hpp>
#include <kernel/interrupts/pic.hpp>
#include <kernel/util/asm_wrap.hpp>
#include <kernel/devices/keyboard.hpp>
#include <kernel/scheduler/init.hpp>

static constexpr unsigned short PIC1 = 0x20; // IO base address for master PIC
static constexpr unsigned short PIC2 = 0xA0; // IO base address for slave PIC
static constexpr unsigned short PIC1_CMD  = PIC1;
static constexpr unsigned short PIC1_DATA = PIC1 + 1;
static constexpr unsigned short PIC2_CMD  = PIC2;
static constexpr unsigned short PIC2_DATA = PIC2 + 1;
static constexpr unsigned short KB_PORT = 0x60;
constexpr unsigned char PIC_EOI = 0x20;  // end of interrupt, sent at end of IRQ-based interrupt routine

static void send_end_of_interrupt(uint interrupt) {
    if(interrupt >= 8) {
        asm_outb(PIC2_CMD, PIC_EOI);
    }
    asm_outb(PIC1_CMD, PIC_EOI);
}

static void pic_interrupt_handler(interrupts::interrupt_args &args) {
    uint num = args.interrupt_number - 0x20;
    if (num == 0) {
        // timer interrupt handler - should run roughly 1000 times per second
        send_end_of_interrupt(num);
        scheduler::timeslice_passed(args);  // NOTE: might not return
        return;
    } else if (num == 1) {
        unsigned char scan_code = asm_inb(KB_PORT);
        devices::keyboard::on_scan_code(scan_code);
    } else {
        kpanic("Invalid PIC interrupt ", num);
    }

    send_end_of_interrupt(num);
}

// default IRQs for master pic and slave pic are 0 to 7 and 8 to 15
// we should remap it to 0x20 to 0x2f so that it won't collide with x86 exceptions
void interrupts::init_pic() {
    // register interrupt handler for all interrupt codes
    for (uint i = 0x20; i < 0x30; i++) {
        register_handler(i, pic_interrupt_handler);
    }

    // ICW1
    asm_outb(PIC1, 0x11);       // Master port A
    asm_outb(PIC2, 0x11);       // Slave port A
    // ICW2
    asm_outb(PIC1_DATA, 0x20);  // Master offset of 0x20 in the IDT
    asm_outb(PIC2_DATA, 0x28);  // Master offset of 0x28 in the IDT
    // ICW3
    asm_outb(PIC1_DATA, 0x04);  // Slaves attached to IR line 2
    asm_outb(PIC2_DATA, 0x02);  // This slave in IR line 2 of master
    // ICW4
    asm_outb(PIC1_DATA, 0x05);  // Set as master
    asm_outb(PIC2_DATA, 0x01);  // Set as slave

    // set masks - 1 is ignored
    asm_outb(PIC1_DATA, 0xfc);  // master PIC - allow only timer and keyboard
    asm_outb(PIC2_DATA, 0xff);  // slave PIC  - block all

    // setup timer
    int divisor = 1193180 / 1000;  // 1000Hz
    asm_outb(0x43, 52);
    asm_outb(0x40, divisor & 0xFF);
    asm_outb(0x40, divisor >> 8);
}
