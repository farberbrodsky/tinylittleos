#include <kernel/util.hpp>
#include <kernel/util/string.hpp>

template <class T>
static inline string_buf dec_impl(T val, char *out) {
    if (val == 0) {
        out[0] = '0';
        out[1] = '\0';
        return string_buf {out, 1};
    }

    size_t i = 0;
    bool negative = false;

    if (val < 0) {
        val = -val;
        negative = true;
    }

    while (val != 0) {
        int remainder = val % 10;
        val = val / 10;
        out[i++] = '0' + remainder;
    }

    if (negative) {
        out[i++] = '-';
    }

    // reverse the string
    for (size_t j = 0; j < (i >> 1); j++) {
        char tmp = out[j];
        out[j] = out[i - j - 1];
        out[i - j - 1] = tmp;
    }

    out[i] = '\0';
    return string_buf {out, i};
}

string_buf str_util::dec(int val, char *out) {
    return dec_impl(val, out);
}

string_buf str_util::dec(uint val, char *out) {
    return dec_impl(val, out);
}

string_buf str_util::hex(uintptr_t val, char *out) {
    if (val == 0) {
        out[0] = '0';
        out[1] = '\0';
        return string_buf {out, 1};
    }

    size_t i = 0;

    while (val != 0) {
        uint remainder = (uintptr_t)val & 0b1111;
        val = val >> 4;
        if (remainder < 10) {
            out[i++] = '0' + remainder;
        } else {
            out[i++] = ('a' - 10) + remainder;
        }
    }

    // reverse the string
    for (size_t j = 0; j < (i >> 1); j++) {
        char tmp = out[j];
        out[j] = out[i - j - 1];
        out[i - j - 1] = tmp;
    }

    out[i] = '\0';
    return string_buf {out, i};
}
