#pragma once
#include <kernel/util.hpp>
#include <kernel/util/string.hpp>
#include <kernel/memory/slab.hpp>

namespace fs {
    // global constants
    constexpr inline uint32_t PATH_NAME_MAX = 400;  // including null byte

    extern memory::slab_allocator<char[PATH_NAME_MAX]> path_name_alloc;

    struct vfs;

    // inode->i_mode flags

    inline uint16_t S_IFMT  = 0170000;  // file type bit mask
    inline uint16_t S_IFREG = 0100000;  // regular file
    inline uint16_t S_IFDIR = 0040000;  // directory
    inline uint16_t S_IFCHR = 0020000;  // character device

    inline uint16_t S_ISUID = 0004000;  // set-user-id
    inline uint16_t S_ISGID = 0002000;  // set-group-id
    inline uint16_t S_ISVTX = 0001000;  // sticky

    inline uint16_t S_IRUSR = 0000400;  // user-read
    inline uint16_t S_IWUSR = 0000200;  // user-write
    inline uint16_t S_IXUSR = 0000100;  // user-exec

    inline uint16_t S_IRGRP = 0000040;  // group-read
    inline uint16_t S_IWGRP = 0000020;  // group-write
    inline uint16_t S_IXGRP = 0000010;  // group-exec

    inline uint16_t S_IROTH = 0000004;  // other-read
    inline uint16_t S_IWOTH = 0000002;  // other-write
    inline uint16_t S_IXOTH = 0000001;  // other-exec

    // the in-memory representation of a file
    struct inode {
        vfs *owner_fs;
        uint16_t i_mode;
        uint32_t i_uid;
        uint32_t i_gid;
        uint32_t i_link_count;
        uint64_t i_size;
        uint32_t i_num;
        uint32_t i_dev;  // device identifier

        // TODO TODO TODO when preempting arrives add reader-writer lock for unlink,create and lookup
        // directories only
        virtual errno lookup(string_buf name, inode &result) = 0;
        virtual errno create(string_buf name, uint16_t mode, inode &result) = 0;
        virtual errno unlink(string_buf name) = 0;
    };

    // file_desc->f_mode flags

    inline uint16_t FMODE_READ  = 0b01;
    inline uint16_t FMODE_WRITE = 0b10;

    // an open file description, which may have several file descriptors in processes
    struct file_desc {
        inode *owner_inode;
        uint32_t f_parent_inode_num;  // may be different even if inode is the same due to hardlinks

        uint16_t f_mode;
        uint64_t f_pos;

        // files only
        virtual ssize_t read(char *buf, size_t count) = 0;
        virtual ssize_t write(char *buf, size_t count) = 0;
        // directories only
        // virtual void iterate() = 0;  // for implementing getdents
    };

    struct vfs {
        // in-memory
        virtual inode *alloc_inode_struct() = 0;
        virtual void free_inode_struct(inode *) = 0;
        // disk
        virtual void write_inode_disk(inode *) = 0;
        virtual void delete_inode_disk(inode *) = 0;

        // used internally to map inode number to inode struct
        void *__hashtable;
    };

    void mount(string_buf canonical_path, vfs *fs);
    void initialize();
}
