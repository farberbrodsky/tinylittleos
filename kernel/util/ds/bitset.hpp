#pragma once
#include <kernel/util.hpp>

namespace ds {
    template <size_t bits>
    struct bitset {
    private:
        static constexpr inline size_t int_count = (bits + 31) / 32;
        uint32_t m_set[int_count];
    public:
        inline constexpr bitset() : m_set { 0 } {}
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
        inline constexpr uint32_t find_bit() {
            for (uint32_t i = 0; i < int_count; i++) {
                if (m_set[i] != 0) {
                    return (i << 5) + __builtin_ctz(m_set[i]);
                }
            }
            return -1;
        }

        inline constexpr void set_all_until(uint32_t index) {
            // NOTE: Assume all other bits are cleared before calling
            uint32_t ints_before = index >> 5;  // 31 has 0 ints before
            for (uint32_t i = 0; i < ints_before; i++) {
                m_set[i] = 0xFFFFFFFF;
            }
            uint32_t remainder = index & 0x1f;
            m_set[ints_before] = (1 << remainder) - 1;  // 3 -> 0b1000 -> 0b0111
        }
        inline constexpr void clear_all() {
            for (uint32_t i = 0; i < int_count; i++) {
                m_set[i] = 0;
            }
        }
        inline constexpr bool all_set_until(uint32_t index) {
            uint32_t ints_before = index >> 5;  // 31 has 0 ints before
            for (uint32_t i = 0; i < ints_before; i++) {
                if (m_set[i] != 0xFFFFFFFF) return false;
            }
            uint32_t remainder = index & 0x1f;
            uint32_t mask = (1 << remainder) - 1;
            if ((m_set[ints_before] & mask) != mask) return false;
            return true;
        }
        inline constexpr bool all_clear_until(uint32_t index) {
            uint32_t ints_before = index >> 5;  // 31 has 0 ints before
            for (uint32_t i = 0; i < ints_before; i++) {
                if (m_set[i] != 0) return false;
            }
            uint32_t remainder = index & 0x1f;
            uint32_t mask = (1 << remainder) - 1;
            if ((m_set[ints_before] & mask) != 0) return false;
            return true;
        }
        inline constexpr bool all_clear() {
            for (uint32_t i = 0; i < int_count; i++) {
                if (m_set[i] != 0) return false;
            }
            return true;
        }

        bitset(const bitset &other) = default;
        bitset &operator=(const bitset &other) = default;
        bitset(bitset &&other) = delete;
        bitset &operator=(bitset &&other) = delete;
    };
}
