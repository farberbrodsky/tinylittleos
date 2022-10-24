#include <kernel/util/ds/hashtable.hpp>

// define global allocator
memory::slab_allocator<ds::__ht_link> ds::__hashtable_link_alloc;
