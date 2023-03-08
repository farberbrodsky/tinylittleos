#include <kernel/interrupts/init.hpp>
#include <kernel/util.hpp>
#include <kernel/tty.hpp>
#include <kernel/logging.hpp>
#include <kernel/util/lock.hpp>
#include <kernel/util/asm_wrap.hpp>

extern "C" {
    extern char interrupt_handler_0;
    extern char interrupt_handler_1;
    extern char interrupt_handler_2;
    extern char interrupt_handler_3;
    extern char interrupt_handler_4;
    extern char interrupt_handler_5;
    extern char interrupt_handler_6;
    extern char interrupt_handler_7;
    extern char interrupt_handler_8;
    extern char interrupt_handler_9;
    extern char interrupt_handler_10;
    extern char interrupt_handler_11;
    extern char interrupt_handler_12;
    extern char interrupt_handler_13;
    extern char interrupt_handler_14;
    extern char interrupt_handler_15;
    extern char interrupt_handler_16;
    extern char interrupt_handler_17;
    extern char interrupt_handler_18;
    extern char interrupt_handler_19;
    extern char interrupt_handler_20;
    extern char interrupt_handler_21;
    extern char interrupt_handler_22;
    extern char interrupt_handler_23;
    extern char interrupt_handler_24;
    extern char interrupt_handler_25;
    extern char interrupt_handler_26;
    extern char interrupt_handler_27;
    extern char interrupt_handler_28;
    extern char interrupt_handler_29;
    extern char interrupt_handler_30;
    extern char interrupt_handler_31;
    extern char interrupt_handler_32;
    extern char interrupt_handler_33;
    extern char interrupt_handler_34;
    extern char interrupt_handler_35;
    extern char interrupt_handler_36;
    extern char interrupt_handler_37;
    extern char interrupt_handler_38;
    extern char interrupt_handler_39;
    extern char interrupt_handler_40;
    extern char interrupt_handler_41;
    extern char interrupt_handler_42;
    extern char interrupt_handler_43;
    extern char interrupt_handler_44;
    extern char interrupt_handler_45;
    extern char interrupt_handler_46;
    extern char interrupt_handler_47;
    extern char interrupt_handler_128;
}

struct __attribute__((packed)) idt_entry {
	uint16_t    isr_low;      // The lower 16 bits of the ISR's address
	uint16_t    kernel_cs;    // The GDT segment selector that the CPU will load into CS before calling the ISR
	uint8_t     reserved;     // Set to zero
	uint8_t     attributes;   // Type and attributes; see the IDT page
                              // Interrupt Gate: 0x8e, Trap Gate: 0x8f
	uint16_t    isr_high;     // The higher 16 bits of the ISR's address
};

static_assert(sizeof(idt_entry) == 8, "wrong idt entry size");

__attribute__((aligned(0x10)))
idt_entry idt_arr[256];  // global

static int interrupt_context_depth;

struct {
    unsigned short size;
    unsigned int address;
} __attribute__((packed, aligned(4))) idtr;  // global

static void (*interrupt_handler_table[256])(interrupts::interrupt_args &arg);  // global

void interrupts::register_handler(uint interrupt, void (*interrupt_handler)(interrupt_args &args)) {
    kassert(interrupt < 48);
    interrupt_handler_table[interrupt] = interrupt_handler;
}

void interrupts::initialize() {
    // zero out interrupt handler table
    memset(interrupt_handler_table, 0, sizeof(interrupt_handler_table));

    // load idt values
    TINY_INFO("Loading IDT");
    char *asm_isr_table[] = { &interrupt_handler_0, &interrupt_handler_1, &interrupt_handler_2, &interrupt_handler_3, &interrupt_handler_4, &interrupt_handler_5, &interrupt_handler_6, &interrupt_handler_7, &interrupt_handler_8, &interrupt_handler_9, &interrupt_handler_10, &interrupt_handler_11, &interrupt_handler_12, &interrupt_handler_13, &interrupt_handler_14, &interrupt_handler_15, &interrupt_handler_16, &interrupt_handler_17, &interrupt_handler_18, &interrupt_handler_19, &interrupt_handler_20, &interrupt_handler_21, &interrupt_handler_22, &interrupt_handler_23, &interrupt_handler_24, &interrupt_handler_25, &interrupt_handler_26, &interrupt_handler_27, &interrupt_handler_28, &interrupt_handler_29, &interrupt_handler_30, &interrupt_handler_31, &interrupt_handler_32, &interrupt_handler_33, &interrupt_handler_34, &interrupt_handler_35, &interrupt_handler_36, &interrupt_handler_37, &interrupt_handler_38, &interrupt_handler_39, &interrupt_handler_40, &interrupt_handler_41, &interrupt_handler_42, &interrupt_handler_43, &interrupt_handler_44, &interrupt_handler_45, &interrupt_handler_46, &interrupt_handler_47 };

    memset(idt_arr, 0, sizeof(idt_arr));
    for (int i = 0; i < 48; i++) {
        // any entries that are not present will generate a General Protection Fault which is itself an interrupt
        idt_arr[i].kernel_cs = 0x08;
        idt_arr[i].isr_low  = reinterpret_cast<uint32_t>(asm_isr_table[i]) & 0xFFFF;
        idt_arr[i].isr_high = reinterpret_cast<uint32_t>(asm_isr_table[i]) >> 16;
        idt_arr[i].reserved = 0;
        idt_arr[i].attributes = 0x8e;  // Interrupt Gate, not Trap Gate
    }
    idt_arr[0x80].kernel_cs = 0x08;
    idt_arr[0x80].isr_low  = reinterpret_cast<uint32_t>(&interrupt_handler_128) & 0xFFFF;
    idt_arr[0x80].isr_high = reinterpret_cast<uint32_t>(&interrupt_handler_128) >> 16;
    idt_arr[0x80].reserved = 0;
    idt_arr[0x80].attributes = 0xee;  // Interrupt Gate, DPL=11

    idtr.address = reinterpret_cast<uint32_t>(idt_arr);
    idtr.size = sizeof(idt_arr) - 1;
    interrupt_context_depth = 0;
}

void interrupts::start() {
    asm_lidt(&idtr);
    sti();
}

void interrupts::reduce_interrupt_depth() {
    int dep = __atomic_sub_fetch(&interrupt_context_depth, 1, __ATOMIC_SEQ_CST);
    kassert(dep >= 0);
}

bool interrupts::is_interrupt_context() {
    return interrupt_context_depth != 0;
}

int interrupts::get_interrupt_context_depth() {
    return interrupt_context_depth;
}

extern "C" void internal_interrupt_handler(interrupts::interrupt_args *arg) {
    __atomic_add_fetch(&interrupt_context_depth, 1, __ATOMIC_SEQ_CST);

    if (interrupt_handler_table[arg->interrupt_number] == nullptr) [[unlikely]] {
        reg_t cr2;
        asm volatile("movl %%cr2, %0" : "=r"(cr2));

        using formatting::hex;
        TINY_ERR("UNHANDLED INTERRUPT !!!\nNUM ", arg->interrupt_number, "\nEBP ", hex{arg->ebp}, "\nEDI ", hex{arg->edi}, "\nESI ", hex{arg->esi}, "\nEDX ", hex{arg->edx}, "\nECX ", hex{arg->ecx}, "\nEBX ", hex{arg->ebx}, "\nEAX ", hex{arg->eax}, "\nCR2 ", hex{cr2}, "\nERROR CODE ", hex{arg->error_code}, "\nEIP ", hex{arg->eip}, "\nCS ", hex{arg->cs}, "\nEFLAGS ", hex{arg->eflags});
        kpanic("Unhandled interrupt: ", arg->interrupt_number, " error code ", arg->error_code, " (0x", hex{arg->error_code}, ')');
        return;
    }
    interrupt_handler_table[arg->interrupt_number](*arg);

    kassert(__atomic_sub_fetch(&interrupt_context_depth, 1, __ATOMIC_SEQ_CST) >= 0);
}
