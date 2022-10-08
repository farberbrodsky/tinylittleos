#pragma once
#include <kernel/util.hpp>

int memcmp(const void *, const void *, size_t);
void* memcpy(void *__restrict, const void *__restrict, size_t);
void* memmove(void *, const void *, size_t);
void* memset(void *, int, size_t);
size_t strlen(const char *);

struct string_buf {
    const char *data;
    size_t length;

    string_buf(const char *data) : data{data} {
        length = strlen(data);
    }
    string_buf(char *data) : string_buf{const_cast<const char *>(data)} {}

    string_buf(const char *data, size_t length) : data{data}, length{length} {}
    string_buf(char *data, size_t length) : string_buf{const_cast<const char *>(data), length} {}
};

using int_to_str_stack_buf = char[21];  // -2**63 with space for null byte in decimal

namespace str_util {
    string_buf dec(int val, char *out);
    string_buf dec(uint val, char *out);
    string_buf hex(uintptr_t val, char *out);
}
