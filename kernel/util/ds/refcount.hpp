#pragma once
#include <kernel/util.hpp>

namespace ds {
    struct intrusive_refcount {
    private:
        int32_t rc;
    public:
        // all objects have 1 reference when constructed
        inline intrusive_refcount() : rc(1) {}

        inline void take_ref() {
            kassert(rc > 0);
            ++rc;
        }

        // returns true if this was the last reference
        inline bool release_ref() {
            kassert(rc > 0);
            if ((--rc) == 0) {
                return true;
            } else {
                return false;
            }
        }
    };
}
