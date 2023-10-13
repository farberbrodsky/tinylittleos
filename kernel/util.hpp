#pragma once
#include <stdint.h>

using ssize_t = int32_t;
using size_t = uint32_t;
using uint = unsigned int;
using reg_t = size_t;
using pid_t = uint32_t;

#define offsetof(type, member) ((size_t)(&((type *)0)->member))
#define container_of(ptr, type, member) ((type *)((char *)(ptr) - offsetof(type, member))) 

void __kassert_fail_internal(const char *assertion, const char *file, uint line, const char *function);

#define kassert(expr) do { __kassert_internal(expr, #expr, __FILE__, __LINE__, __PRETTY_FUNCTION__); } while (0);
inline void __kassert_internal(bool expr, const char *assertion, const char *file, uint line, const char *function) {
    if (!expr) [[unlikely]] {
        __kassert_fail_internal(assertion, file, line, function);
    }
}

namespace interrupts {
    bool is_interrupt_context();
}
#define kassert_not_interrupt kassert(!interrupts::is_interrupt_context());
#define kassert_is_interrupt kassert(interrupts::is_interrupt_context());

#define kpanic(msg...) do {                                           \
    __kpanic_internal_before();                                       \
    tty_driver::write(msg);                                           \
    __kpanic_internal_after(__FILE__, __LINE__, __PRETTY_FUNCTION__); \
} while (0);

void __kpanic_internal_before();
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
enum class errno : ssize_t {
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
    invalid = -22,       // EINVAL

    // tinylittleos extensions
    path_too_long = -1337,
};

// C++ features

// std::move and std::forward
namespace util {
    template <class T> struct remove_reference      { using type = T; };
    template <class T> struct remove_reference<T&>  { using type = T; };
    template <class T> struct remove_reference<T&&> { using type = T; };
    template <class T> using  remove_reference_t = typename remove_reference<T>::type;
    template <class T>
    inline constexpr remove_reference_t<T>&& move(T&& t) noexcept {
        return static_cast<remove_reference_t<T>&&>(t);
    }
    template <class T>
    inline constexpr T&& forward(remove_reference_t<T>&& t) noexcept {
        return static_cast<T&&>(t);
    }
}

// placement new
inline void *operator new(size_t, void *p)     noexcept { return p; }
inline void *operator new[](size_t, void *p)   noexcept { return p; }
inline void  operator delete  (void *, void *) noexcept { };
inline void  operator delete[](void *, void *) noexcept { };

// once we have all of the defines, tty can and should be loaded
#include <kernel/tty.hpp>
