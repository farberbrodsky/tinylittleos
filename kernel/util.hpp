#pragma once
#include <stdint.h>

using size_t = uintptr_t;
using uint = unsigned int;
using reg_t = size_t;
void __kassert_fail_internal(const char *assertion, const char *file, uint line, const char *function);

#define kassert(expr) __kassert_internal(expr, #expr, __FILE__, __LINE__, __PRETTY_FUNCTION__);
inline void __kassert_internal(bool expr, const char *assertion, const char *file, uint line, const char *function) {
    if (!expr) {
        __kassert_fail_internal(assertion, file, line, function);
    }
}

#define kpanic(msg) __kpanic_internal(msg, __FILE__, __LINE__, __PRETTY_FUNCTION__);
void __kpanic_internal(const char *msg, const char *file, uint line, const char *function);

inline void quality_debugging() {
    for (int i = 0; i < 10000000; i++) {
        asm("pause");
    }
}

#define kunused(var) ((void)(var))
