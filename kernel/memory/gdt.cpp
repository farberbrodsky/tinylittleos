#include <kernel/util.hpp>
#include <kernel/memory/gdt.hpp>
#include <kernel/util/asm_wrap.hpp>
#include <kernel/scheduler/init.hpp>
using scheduler::global_tss;

struct gdt_entry {
    uint32_t base;
    uint32_t limit;
    uint8_t access_byte;
    uint8_t flags;
};

struct encoded_gdt_entry {
    uint8_t a[8];
};

template <size_t n>
struct __attribute__((packed)) encoded_gdt_array {
    uint8_t a[8 * n];
};

template <class... Args>
static consteval auto concat_gdt_entries(Args... args) {
    encoded_gdt_entry args_cast[] = { static_cast<encoded_gdt_entry>(args)... };
    constexpr size_t args_len = sizeof(args_cast) / sizeof(args_cast[0]);
    encoded_gdt_array<args_len> result;

    for (size_t i = 0; i < args_len; i++) {
        for (int j = 0; j < 8; j++) {
            result.a[i * 8 + j] = args_cast[i].a[j];
        }
    }

    return result;
}

static consteval encoded_gdt_entry gdt_entry(gdt_entry source) {
    encoded_gdt_entry result;
 
    // Encode the limit
    result.a[0] = source.limit & 0xFF;
    result.a[1] = (source.limit >> 8) & 0xFF;
    result.a[6] = (source.limit >> 16) & 0x0F;
 
    // Encode the base
    result.a[2] = source.base & 0xFF;
    result.a[3] = (source.base >> 8) & 0xFF;
    result.a[4] = (source.base >> 16) & 0xFF;
    result.a[7] = (source.base >> 24) & 0xFF;
 
    // Encode the access byte
    result.a[5] = source.access_byte;
 
    // Encode the flags
    result.a[6] |= (source.flags << 4);
    return result;
}

// access byte:
// 7 - Present
// 5,6 - DPL
// 4 - type, 1 if code or data 0 if tss
// 3 - exec
// 2 - direction, 0 grows up
// 1 - read/write. code: read access, data: write access
// 0 - accessed, set to 0

// flags:
// 3 - Granularity. 1 is page granularity 0 is byte granularity
// 2 - 1 if 32-bit segment 0 if 16-bit segment
// 1 - 1 if 64-bit code segment
// 0 - reserved

__attribute__((aligned(4)))
encoded_gdt_array<6> gdt_arr { concat_gdt_entries(  // global
    // null descriptor
    gdt_entry({ .base = 0, .limit = 0,       .access_byte = 0,          .flags = 0      }),
    // kernel cs
    gdt_entry({ .base = 0, .limit = 0xFFFFF, .access_byte = 0b10011010, .flags = 0b1100 }),
    // kernel ds
    gdt_entry({ .base = 0, .limit = 0xFFFFF, .access_byte = 0b10010010, .flags = 0b1100 }),
    // user cs
    gdt_entry({ .base = 0, .limit = 0xFFFFF, .access_byte = 0b11111010, .flags = 0b1100 }),
    // user ds
    gdt_entry({ .base = 0, .limit = 0xFFFFF, .access_byte = 0b11110010, .flags = 0b1100 }),
    // TSS
    gdt_entry({ .base = 0, .limit = sizeof(global_tss), .access_byte = 0b10001001, .flags = 0 })) };

struct {
    unsigned short size;
    unsigned int address;
} __attribute__((packed, aligned(4))) gdtr;  // global

void memory::init_gdt() {
    // make sure LDT is cleared
    gdtr.address = 0;
    gdtr.size = 0;
    asm_lldt(&gdtr);
    // set GDT
    uint32_t tss_addr = (uint32_t)&global_tss;
    gdt_arr.a[0x28 + 2] = tss_addr & 0xFF;
    gdt_arr.a[0x28 + 3] = (tss_addr >> 8) & 0xFF;
    gdt_arr.a[0x28 + 4] = (tss_addr >> 16) & 0xFF;
    gdt_arr.a[0x28 + 7] = (tss_addr >> 24) & 0xFF;
    gdtr.address = reinterpret_cast<unsigned int>(&gdt_arr.a);
    gdtr.size = sizeof(gdt_arr) - 1;
    asm_lgdt(&gdtr);
    // set TSS
    asm_flush_tss();
}
