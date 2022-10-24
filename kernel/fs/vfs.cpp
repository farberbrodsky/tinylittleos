#include <kernel/fs/vfs.hpp>
#include <kernel/memory/slab.hpp>
#include <kernel/util/string.hpp>
#include <kernel/util/ds/list.hpp>
#include <kernel/util/ds/hashtable.hpp>
using namespace fs;

memory::slab_allocator<char[PATH_NAME_MAX]> path_name_alloc;

// mount points are implemented as a linked list of path names we have to compare to
struct mount_point : ds::intrusive_doubly_linked_node<mount_point> {
    char m_path[PATH_NAME_MAX];
    size_t m_path_length;
    vfs *m_fs;

    inline mount_point(string_buf path, vfs *fs) : m_fs(fs) {
        kassert(path.length < PATH_NAME_MAX);
        memcpy(m_path, path.data, path.length + 1);
        m_path_length = path.length;
    }
};

static memory::slab_allocator<mount_point> mount_point_alloc;
static mount_point *first_mount_point = nullptr;

void fs::mount(string_buf canonical_path, vfs *fs) {
    // allocate hash table
    static_assert(sizeof(ds::hashtable<256>) == 4096, "wrong hash table size");
    fs->__hashtable = memory::kmem_alloc_4k();
    new (fs->__hashtable) ds::hashtable<256>();  // placement new - call constructor

    // put in mount point linked list
    mount_point *m = mount_point_alloc.allocate(canonical_path, fs);
    if (first_mount_point == nullptr)
        first_mount_point = m;
    else
        first_mount_point->add_after_self(m);
}

void fs::initialize() {
    // TODO do nothing...
}
