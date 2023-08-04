#pragma once
namespace memory {
    constexpr int user_cs = 0x1b;
    constexpr int kernel_cs = 0x08;
    void init_gdt();
}
