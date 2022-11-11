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

static errno tar_lookup(char *archive, string_buf filename, uint32_t &inode, char *&content, uint32_t &file_len) {
    if (filename.length > 99) return errno::path_too_long;
    char *ptr = archive;
    inode = 3;  // not root
    while (!memcmp(ptr + 257, "ustar", 5)) {
        uint32_t filesize = parse_oct((unsigned char *)ptr + 0x7c, 11);
        char last = ptr[filename.length];
        if ((!memcmp(ptr, filename.data, filename.length) && (last == '\0' || last == '/'))) {
            // found file!
            content = ptr + 512;
            if (last == '\0') {  // not directory
                file_len = filesize;
            } else {
                file_len = 0;
            }
            // additionally, add 1 to inode for each / in the name, to identify directories
            for (size_t i = 0; i < filename.length; i++) {
                if (filename.data[i] == '/') inode++;
            }

            return errno::ok;
        }

        // additionally, add 1 to inode for each / in the full name, to identify directories
        char *slash_ptr = ptr;
        while (*slash_ptr != '\0') {
            if (*(slash_ptr++) == '/')
                inode++;
        }

        ptr += (((filesize + 511) / 512) + 1) * 512;
        inode++;
    }
    return errno::no_entry;
}

static errno tar_lookup_by_index(char *archive, uint32_t inode, size_t &filename_length, char *&content, uint32_t &file_len) {
    char *ptr = archive;
    if (inode < 3) return errno::no_entry;
    uint32_t curr_inode = 3;
    while (!memcmp(ptr + 257, "ustar", 5)) {
        uint32_t filesize = parse_oct((unsigned char *)ptr + 0x7c, 11);

        // add 1 to inode for each / in the full name, to identify directories
        char *slash_ptr = ptr;
        while (*slash_ptr != '\0') {
            if (curr_inode == inode) {
                // found!
                // keep going until next slash for full filename length
                while (*slash_ptr != '\0' && *slash_ptr != '/')
                    slash_ptr++;
                filename_length = slash_ptr - ptr;

                content = ptr + 512;
                file_len = filesize;
                return errno::ok;
            }

            if (*(slash_ptr++) == '/')
                curr_inode++;
        }

        ptr += (((filesize + 511) / 512) + 1) * 512;
        curr_inode++;
    }
    return errno::no_entry;
}

struct vfs_tar : public vfs {
    virtual inode *alloc_root_inode_struct() override;
    virtual inode *alloc_inode_struct(uint32_t) override;
    virtual void free_inode_struct(inode *) override;
    virtual void read_inode_disk(inode *) override;
    virtual void write_inode_disk(inode *) override;
    virtual void delete_inode_disk(inode *) override;

    char *m_archive;
    vfs_tar(char *archive);
    ~vfs_tar();
};

struct inode_tar : public inode {
    virtual errno lookup(string_buf name, uint32_t &found_i) override;
    virtual errno create(string_buf name, uint16_t mode, uint32_t &new_i) override;
    virtual errno unlink(string_buf name) override;

    char *m_contents;            // start of contents in tar - header is 512 before that
    uint32_t m_filename_length;  // how much of filename is relevant

    inode_tar(vfs *owner);
    ~inode_tar();
};

struct file_desc_tar : public file_desc {
    virtual ssize_t read(char *buf, size_t count) override;
    virtual ssize_t write(char *buf, size_t count) override;
    ~file_desc_tar();
};

static memory::slab_allocator<inode_tar> inode_alloc;


// vfs definitiions


vfs_tar::vfs_tar(char *archive) : m_archive(archive) {}
vfs_tar::~vfs_tar() {}

inode *vfs_tar::alloc_root_inode_struct() {
    inode *result = inode_alloc.allocate(this);
    result->i_num = root_inode;
    return result;
}

inode *vfs_tar::alloc_inode_struct(uint32_t i_num) {
    inode *result = inode_alloc.allocate(this);
    result->i_num = i_num;
    return result;
}

void vfs_tar::free_inode_struct(inode *node) {
    inode_alloc.free(static_cast<inode_tar *>(node));
}

void vfs_tar::read_inode_disk(inode *node) {
    inode_tar *cast_node = static_cast<inode_tar *>(node);
    uint32_t number = node->i_num;
    uint32_t _file_len;
    tar_lookup_by_index(m_archive, number, cast_node->m_filename_length, cast_node->m_contents, _file_len);
}

void vfs_tar::write_inode_disk(inode *) {
    kpanic("a"); // TODO
}

void vfs_tar::delete_inode_disk(inode *) {
    kpanic("a"); // TODO
}


// inode definitions


inode_tar::inode_tar(vfs *owner) : inode(owner), m_contents(nullptr) {}
inode_tar::~inode_tar() {}

errno inode_tar::lookup(string_buf name_arg, uint32_t &found_i) {
    string_buf my_file_name(static_cast<char *>(nullptr), 0);

    if (i_num != 2) {
        // am not root - get real file name (root has empty file name)
        kassert(this->m_contents != nullptr);
        char *my_header = this->m_contents - 512;
        my_file_name = string_buf(my_header, m_filename_length);
        // cut trailing slashes
        while (my_file_name.length > 0 && my_file_name.data[my_file_name.length - 1] == '/') {
            my_file_name.length--;
        }
        while (my_file_name.length > 0 && my_file_name.data[0] == '/') {
            my_file_name.data++;
            my_file_name.length--;
        }
    }

    //                  my_file_name          /   name_arg
    size_t concat_len = my_file_name.length + 1 + name_arg.length;
    if (my_file_name.length == 0) concat_len--;  // no / in this case

    if ((concat_len + 1) > PATH_NAME_MAX) {  // +1 is for \0
        return errno::path_too_long;
    }

    char concat_path_name_buf[PATH_NAME_MAX];
    char *concat_pos = concat_path_name_buf;

    // my_file_name
    memcpy(concat_pos, my_file_name.data, my_file_name.length);
    concat_pos += my_file_name.length;

    if (my_file_name.length != 0) {
        // /
        *(concat_pos++) = '/';
    }

    // name_arg
    memcpy(concat_pos, name_arg.data, name_arg.length);
    concat_pos += name_arg.length;

    // \0
    *(concat_pos++) = '\0';

    // look for this string
    string_buf concat_path_name(concat_path_name_buf, concat_len);

    // mandatory references to call tar_lookup
    char *_content;
    uint32_t _file_len;
    errno e = tar_lookup(static_cast<vfs_tar *>(owner_fs)->m_archive, concat_path_name, found_i, _content, _file_len);

    if (e != errno::ok)
        return e;

    return errno::ok;
}

errno inode_tar::create(string_buf, uint16_t, uint32_t &) {
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

static char static_vfs_allocation[sizeof(vfs_tar)];

void fs::register_tar() {
    vfs_tar *fs = new (static_vfs_allocation) vfs_tar(&_initrd);
    fs::mount("/", fs);
}