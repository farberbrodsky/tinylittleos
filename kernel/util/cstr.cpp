#include <kernel/util.hpp>
#include <kernel/util/string.hpp>

size_t strlen(const char *s) {
    size_t len = 0;
    while (s[len])
        len++;
    return len;
}
