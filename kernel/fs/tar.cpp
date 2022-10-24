#include <kernel/fs/tar.hpp>
#include <kernel/tty.hpp>
#include <kernel/logging.hpp>
using namespace fs;

extern char _initrd;

static uint32_t parse_oct(unsigned char *str, uint32_t size) {
    uint32_t n = 0;
    while (size-- > 0) {
        n <<= 3;
        n += *str - '0';
        str++;
    }
    return n;
}

static uint32_t tar_lookup(char *archive, string_buf filename, char *&out) {
    char *ptr = archive;
    while (!memcmp(ptr + 257, "ustar", 5)) {
        uint32_t filesize = parse_oct((unsigned char *)ptr + 0x7c, 11);
        if (!memcmp(ptr, filename.data, filename.length + 1)) {
            out = ptr + 512;
            return filesize;
        }
        ptr += (((filesize + 511) / 512) + 1) * 512;
    }
    return -1;
}

struct vfs_tar : public vfs {
    virtual inode *alloc_inode_struct() override;
    virtual void free_inode_struct(inode *) override;
    virtual void write_inode_disk(inode *) override;
    virtual void delete_inode_disk(inode *) override;
};

struct inode_tar : public inode {
    virtual errno lookup(string_buf name, inode &result) override;
    virtual errno create(string_buf name, uint16_t mode, inode &result) override;
    virtual errno unlink(string_buf name) override;
};

struct file_desc_tar : public file_desc {
    virtual ssize_t read(char *buf, size_t count) override;
    virtual ssize_t write(char *buf, size_t count) override;
};

static memory::slab_allocator<inode_tar> inode_alloc;


// vfs definitiions


inode *vfs_tar::alloc_inode_struct() {
    return inode_alloc.allocate();
}
void vfs_tar::free_inode_struct(inode *node) {
    inode_alloc.free(static_cast<inode_tar *>(node));
}

void vfs_tar::write_inode_disk(inode *) {
    kpanic("a"); // TODO
}

void vfs_tar::delete_inode_disk(inode *) {
    kpanic("a"); // TODO
}


// inode definitions


errno inode_tar::lookup(string_buf, inode &) {
    kpanic("a"); // TODO
    return errno::no_entry;
}

errno inode_tar::create(string_buf, uint16_t, inode &) {
    kpanic("a"); // TODO
    return errno::not_permitted;
}

errno inode_tar::unlink(string_buf) {
    kpanic("a"); // TODO
    return errno::not_permitted;
}


// file desc definitions


ssize_t file_desc_tar::read(char *, size_t) {
    kpanic("a"); // TODO
    return (int)errno::not_permitted;
}

ssize_t file_desc_tar::write(char *, size_t) {
    kpanic("a"); // TODO
    return (int)errno::not_permitted;
}

void fs::register_tar() {
    char *file_content = nullptr;
    uint32_t file_len = tar_lookup(&_initrd, "hello.txt", file_content);
    for (uint32_t i = 0; i < file_len; i++) {
        tty::put(file_content[i]);
    }
}
