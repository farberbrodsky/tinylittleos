#include <kernel/memory/multiboot.hpp>

size_t memory::ram_amount;  // global

void memory::read_multiboot_data(multiboot_info_t *data, uint magic) {
    data = (multiboot_info_t *)((char *)data + 0xC0000000);
    kassert(magic == MULTIBOOT_BOOTLOADER_MAGIC);
    kassert(data->flags >> 6 & 0x1);  // make sure there is a memory map

    for (uint i = 0; i < data->mmap_length; i += sizeof(multiboot_memory_map_t)) {
        auto *mmmt = (multiboot_memory_map_t *)(data->mmap_addr + 0xC0000000 + i);
        if (mmmt->addr == 0x100000) {
            ram_amount = mmmt->len;
        }
    }

    kassert(ram_amount != 0);
}
