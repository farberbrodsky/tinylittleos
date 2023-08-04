#include <kernel/memory/virtual_memory.hpp>

memory::virtual_memory::~virtual_memory() {
    // destroy all vm areas
    kpanic("todo");
}

// static
void memory::virtual_memory::release(memory::virtual_memory *) {
    kpanic("todo");
}
