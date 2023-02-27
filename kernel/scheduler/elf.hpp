#include <kernel/util.hpp>
#include <kernel/fs/vfs.hpp>

namespace elf_loader {
    errno load_elf(fs::file_desc *file, void *&entry);
}
