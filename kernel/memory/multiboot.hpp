#pragma once
extern "C" {
#include <kernel/util/multiboot.h>
}
#include <kernel/util.hpp>

namespace memory {
    extern size_t ram_amount;
    void read_multiboot_data(multiboot_info_t *data, uint magic);
}
