#pragma once
#include <stdint.h>

using ssize_t = int32_t;
using size_t = uint32_t;
using uint = unsigned int;
using reg_t = size_t;
void __kassert_fail_internal(const char *assertion, const char *file, uint line, const char *function);

#define kassert(expr) __kassert_internal(expr, #expr, __FILE__, __LINE__, __PRETTY_FUNCTION__);
inline void __kassert_internal(bool expr, const char *assertion, const char *file, uint line, const char *function) {
    if (!expr) [[unlikely]] {
        __kassert_fail_internal(assertion, file, line, function);
    }
}

#define kpanic(msg...) do {                                           \
    __kpanic_internal_before();                                       \
    tty_driver::write(msg);                                           \
    __kpanic_internal_after(__FILE__, __LINE__, __PRETTY_FUNCTION__); \
} while (0);

void __kpanic_internal_before(void);
void __kpanic_internal_after(const char *file, uint line, const char *function);

inline void quality_debugging() {
    for (int i = 0; i < 10000000; i++) {
        asm("pause");
    }
}

#define kunused(var) ((void)(var))

// TODO TODO TODO remember to make use of __user
# define __user __attribute__((noderef, address_space(1)))

// errno
enum class errno : int {
    ok = 0,
    not_permitted = -1,  // EPERM
    no_entry = -2,       // ENOENT
    no_process = -3,     // ESRCH
    interrupted = -4,    // EINTR
    io_error = -5,       // EIO
    no_memory = -12,     // ENOMEM
    no_access = -13,     // EACCESS
    exists = -17,        // EEXISTS
    not_dir = -20,       // ENOTDIR
    is_dir = -21,        // EISDIR

    // tinylittleos extensions
    path_too_long = -1337,
};

// C++ features

// std::move
namespace std {
    template <class T> struct remove_reference      { typedef T type; };
    template <class T> struct remove_reference<T&>  { typedef T type; };
    template <class T> struct remove_reference<T&&> { typedef T type; };
    template <class T> using  remove_reference_t = typename remove_reference<T>::type;
    template <class T>
    inline constexpr remove_reference_t<T>&& move(T&& t) noexcept {
        return static_cast<remove_reference_t<T>&&>(t);
    }
}

// placement new
inline void *operator new(size_t, void *p)     throw() { return p; }
inline void *operator new[](size_t, void *p)   throw() { return p; }
inline void  operator delete  (void *, void *) throw() { };
inline void  operator delete[](void *, void *) throw() { };
