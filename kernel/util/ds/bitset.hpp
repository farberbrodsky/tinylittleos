#pragma once
#include <kernel/util.hpp>

namespace ds {
    template <size_t bits>
    struct bitset {
    private:
        static constexpr inline size_t int_count = (bits + 31) / 32;
        uint32_t m_set[int_count];
    public:
        bitset() : m_set { 0 } {}
        inline constexpr void set_bit(uint32_t index) {
            uint32_t arr_index = index >> 5;     // division by 32
            uint32_t bit = 1 << (index & 0x1F);  // & 0x1F is modulo 32
            m_set[arr_index] |= bit;
        }
        inline constexpr void clear_bit(uint32_t index) {
            uint32_t arr_index = index >> 5;
            uint32_t bit = 1 << (index & 0x1F);
            m_set[arr_index] &= (~bit);
        }
        inline constexpr bool has_bit(uint32_t index) {
            uint32_t arr_index = index >> 5;
            uint32_t bit = 1 << (index & 0x1F);
            return (m_set[arr_index] & bit) != 0;
        }

        bitset(const bitset &other) = default;
        bitset &operator=(const bitset &other) = default;
        bitset(bitset &&other) = delete;
        bitset &operator=(bitset &&other) = delete;
    };
}
