#include <kernel/util/ds/refcount.hpp>

void ds::intrusive_refcount::take_ref() {
    kassert(rc > 0);
    ++rc;
}

bool ds::intrusive_refcount::release_ref() {
    kassert(rc > 0);
    if ((--rc) == 0) {
        return true;
    } else {
        return false;
    }
}
