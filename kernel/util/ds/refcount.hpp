#pragma once
#include <kernel/util.hpp>

namespace ds {
    struct intrusive_refcount {
    protected:
        int32_t rc;
    public:
        // all objects have 1 reference when constructed
        inline intrusive_refcount() : rc(1) {}
        void take_ref();
        // returns true if this was the last reference
        bool release_ref();

        inline int32_t get_rc_for_test() {
            return rc;
        }
    };
}
