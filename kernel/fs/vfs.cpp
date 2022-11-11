#include <kernel/fs/vfs.hpp>
#include <kernel/memory/slab.hpp>
#include <kernel/util/lock.hpp>
#include <kernel/util/string.hpp>
#include <kernel/util/ds/list.hpp>
#include <kernel/util/ds/hashtable.hpp>
#include <kernel/logging.hpp>
using namespace fs;

memory::untyped_slab_allocator<PATH_NAME_MAX> path_name_alloc;

// mount points are implemented as a linked list of path names we have to compare to
struct mount_point : ds::intrusive_doubly_linked_node<mount_point> {
    char m_path[PATH_NAME_MAX];
    size_t m_path_length;
    vfs *m_fs;

    inline mount_point(string_buf path, vfs *fs) : m_fs(fs) {
        kassert(path.length < PATH_NAME_MAX);
        kassert(path.length != 0);
        kassert(path.length == 1 || (path.data[path.length - 1] != '/'));  // must not end with slash
        memcpy(m_path, path.data, path.length);
        m_path[path.length] = '\0';
        m_path_length = path.length;
    }
};

static memory::slab_allocator<mount_point> mount_point_alloc;
static mount_point *first_mount_point = nullptr;

inode::~inode() {}

file_desc::~file_desc() {}

vfs::~vfs() {}

// mounting implementation

void fs::mount(string_buf canonical_path, vfs *fs) {
    // allocate hash table
    static_assert(sizeof(ds::hashtable<256>) == 4096, "wrong hash table size");
    fs->__hashtable = memory::kmem_alloc_4k();
    ds::hashtable<256> *ht = new (fs->__hashtable) ds::hashtable<256>();  // placement new - call constructor

    ht->insert(fs->root_inode, fs->alloc_root_inode_struct());

    // put in mount point linked list
    mount_point *m = mount_point_alloc.allocate(canonical_path, fs);
    if (first_mount_point == nullptr)
        first_mount_point = m;
    else
        first_mount_point->add_after_self(m);
}

// virtual filesystem operations

// either from hashtable (if possible) or allocate new
// also takes reference to it
static inode *get_inode_struct(vfs *fs, uint32_t i_num) {
    ds::hashtable<256> *ht = reinterpret_cast<ds::hashtable<256> *>(fs->__hashtable);

    inode *result = reinterpret_cast<inode *>(ht->lookup(i_num));

    if (result != nullptr) {
        kassert(i_num == result->i_num);
        result->take_ref();
    } else {
        // need to allocate inode struct
        result = fs->alloc_inode_struct(i_num);  // (comes with 1 reference as needed)
        fs->read_inode_disk(result);  // TODO make this asynchronous and return a non-ready inode struct
        ht->insert(i_num, result);
    }

    return result;
}

void fs::release_inode_struct(inode *i) {
    if (i->release_ref()) {
        ds::hashtable<256> *ht = reinterpret_cast<ds::hashtable<256> *>(i->owner_fs->__hashtable);
        ht->remove(i->i_num);
        i->owner_fs->free_inode_struct(i);
    }
}

errno fs::traverse(string_buf path, inode *&result) {
    // no preemption during fs traverse
    scoped_preemptlock lock;

    if (path.length == 0) return errno::no_entry;

    if (first_mount_point == nullptr) {
        return errno::no_entry;
    }

    // TODO TODO TODO make mount point list sorted by length so this will be faster
    // step 1. find longest matching mount point
    mount_point *best_mnt = nullptr;
    size_t best_mnt_path_length = 0;

    for (mount_point &mnt : *first_mount_point) {
        if ((mnt.m_path_length <= best_mnt_path_length) || (mnt.m_path_length > path.length)) {
            continue;  // can't be a better match or can't match since it is longer than path itself
        }
        // now guaranteed: path.length >= mnt.m_path_length

        if (!(mnt.m_path_length == 1 || path.length == mnt.m_path_length || path.data[mnt.m_path_length] == '/')) {
            continue;  // needed: either no trailing slash or after the mount's length there's a slash, or mnt is root so it always matches
        }

        bool match = true;
        for (size_t i = 0; i < mnt.m_path_length; i++) {
            if (mnt.m_path[i] != path.data[i]) {
                match = false;
                break;
            }
        }
        if (!match) continue;

        // found a match: all the characters until the mount's length match
        // and after the exact mount name there is either nothing or /
        best_mnt = &mnt;
        best_mnt_path_length = best_mnt->m_path_length;  // used to avoid dereferencing null cheaply
    }

    if (best_mnt == nullptr) {
        return errno::no_entry;
    }

    // found mount point, start traversal
    vfs *fs = best_mnt->m_fs;
    ds::hashtable<256> *ht = reinterpret_cast<ds::hashtable<256> *>(fs->__hashtable);

    // start from root inode
    uint32_t root_inode_num = fs->root_inode;
    inode *curr_inode = reinterpret_cast<inode *>(ht->lookup(root_inode_num));
    kassert(curr_inode != nullptr);

    // destructively parse path and traverse inodes
    while (path.length != 0) {
        if (path.data[0] == '/') {
            // ignore slashes
            path.data++;
            path.length--;
            continue;
        }

        // take all text until the next slash
        size_t until_slash = 0;
        while (until_slash < path.length && path.data[until_slash] != '/')
            until_slash++;

        // lookup in the current inode
        uint32_t found_i;
        string_buf name_buf { path.data, until_slash };
        errno e = curr_inode->lookup(name_buf, found_i);

        // release curr_inode (unless it is root)
        if (curr_inode->i_num != root_inode_num) {
            release_inode_struct(curr_inode);
        }

        if (e != errno::ok) {
            return e;
        }
        curr_inode = get_inode_struct(fs, found_i);

        // remove from remaining text
        path.data += until_slash;
        path.length -= until_slash;
    }

    result = curr_inode;
    return errno::ok;
}

// initialization

void fs::initialize() {
    // TODO do nothing...
}
