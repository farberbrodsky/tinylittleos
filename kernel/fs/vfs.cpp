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

static ssize_t default_f_read(file_desc *, char *, size_t, uint64_t) {
    kpanic("file read not defined");
    return static_cast<ssize_t>(errno::not_permitted);
}
static ssize_t default_f_write(file_desc *, char *, size_t, uint64_t) {
    kpanic("file write not defined");
    return static_cast<ssize_t>(errno::not_permitted);
}

file_desc::file_desc(inode *owner) : owner_inode(owner), f_mode(0), f_pos(0), f_read(default_f_read), f_write(default_f_write) {
    owner->take_ref_locked();
}
file_desc::~file_desc() {
    inode::release(owner_inode);
}

ssize_t file_desc::read(char *buf, size_t count) {
    ssize_t res = this->f_read(this, buf, count, f_pos);
    if (res >= 0)
        f_pos += res;

    return res;
}

ssize_t file_desc::pread(char *buf, size_t count, uint64_t pos) {
    return this->f_read(this, buf, count, pos);
}

ssize_t file_desc::write(char *buf, size_t count) {
    return this->f_write(this, buf, count, f_pos);
}

ssize_t file_desc::pwrite(char *buf, size_t count, uint64_t pos) {
    return this->f_write(this, buf, count, pos);
}

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
static inode *get_inode_struct(vfs *fs, uint32_t i_num, inode *parent) {
    ds::hashtable<256> *ht = reinterpret_cast<ds::hashtable<256> *>(fs->__hashtable);

    inode *result = reinterpret_cast<inode *>(ht->lookup(i_num));

    if (result != nullptr) {
        kassert(i_num == result->i_num);
        result->take_ref_locked();
    } else {
        // need to allocate inode struct
        result = fs->alloc_inode_struct(i_num, parent);  // (comes with 1 reference as needed)
        fs->read_inode_disk(result);  // TODO make this asynchronous and return a non-ready inode struct
        ht->insert(i_num, result);
    }

    return result;
}

void fs::inode::release(inode *obj) {
    if (obj->release_ref_locked()) {
        ds::hashtable<256> *ht = reinterpret_cast<ds::hashtable<256> *>(obj->owner_fs->__hashtable);
        ht->remove(obj->i_num);
        obj->owner_fs->free_inode_struct(obj);
    }
}

void fs::take_parent_struct(inode *i) {
    if (i->i_parent_ref) i->i_parent_ref->take_ref_locked();
}

void fs::release_parent_struct(inode *i) {
    if (i->i_parent_ref) inode::release(i->i_parent_ref);
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
    path.data += best_mnt_path_length;
    path.length -= best_mnt_path_length;
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

        if (e != errno::ok) {
            // release curr_inode (unless it is root)
            if (curr_inode->i_num != root_inode_num) {
                inode::release(curr_inode);
            }
            return e;
        }

        inode *prev_inode = curr_inode;
        curr_inode = get_inode_struct(fs, found_i, prev_inode);

        // release prev_inode (unless it is root)
        if (prev_inode->i_num != root_inode_num) {
            inode::release(prev_inode);
        }

        // remove from remaining text
        path.data += until_slash;
        path.length -= until_slash;
    }

    result = curr_inode;
    return errno::ok;
}

static memory::slab_allocator<file_desc> file_desc_alloc;

errno inode::open(file_desc *&result) {
    file_desc *f = file_desc_alloc.allocate(this);
    set_file_methods(f);
    result = f;
    return errno::ok;
}

void fs::file_desc::release(file_desc *obj) {
    if (obj->release_ref()) {
        file_desc_alloc.free(obj);
    }
}

// initialization

void fs::initialize() {
    // TODO do nothing...
}
