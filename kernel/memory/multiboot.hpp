#pragma once
extern "C" {
#include <kernel/util/multiboot.h>
}
#include <kernel/util.hpp>

namespace memory {
    extern size_t ram_amount;
    inline uint32_t ram_amount_start = 0x100000;
    void read_multiboot_data(multiboot_info_t *data, uint magic);
}
