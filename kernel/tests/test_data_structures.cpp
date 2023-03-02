#include <kernel/tty.hpp>
#include <kernel/serial.hpp>
#include <kernel/logging.hpp>
#include <kernel/memory/multiboot.hpp>
#include <kernel/memory/gdt.hpp>
#include <kernel/memory/page_allocator.hpp>
#include <kernel/interrupts/init.hpp>
#include <kernel/interrupts/pic.hpp>
#include <kernel/util/ds/bitset.hpp>
#include <kernel/util/ds/list.hpp>
#include <kernel/util/ds/hashtable.hpp>
#include <kernel/util/ds/refcount.hpp>
#include <kernel/util/ds/rbtree.hpp>
#include <kernel/util/rng.hpp>

static unsigned long seed;

static void test_bitset() {
    // create bitset on non-zeroed memory
    char memory[12];
    memset(memory, 0xFF, sizeof(memory));
    ds::bitset<70> &bits = *(new (&memory) ds::bitset<70>());
    static_assert(sizeof(ds::bitset<70>) == 12);

    for (int i = 0; i < 70; i++) {
        kassert(bits.has_bit(i) == false);
        if ((i % 3) == 0) bits.set_bit(i);
    }
    for (int i = 0; i < 70; i += 3) {
        kassert(bits.has_bit(i) == true);
        bits.clear_bit(i);
    }

    ds::bitset<70> bits2;
    bits2.set_bit(50);
    ds::bitset<70> bits3(bits2);
    kassert(bits2.has_bit(50));
    kassert(bits3.has_bit(50));
    bits2 = bits;
    kassert(!bits2.has_bit(50));
    kassert(bits3.has_bit(50));
    TINY_INFO("Pass test_bitset");
}

static void test_linked_list() {
    struct A : ds::intrusive_doubly_linked_node<A> {
        int m_x;
        inline A(int x) : m_x(x) {};
    };

    A a(7);
    A b(4);
    A c(2);
    A d(3);
    A e(1);
    a.add_after_self(&b);  // a, b
    a.add_after_self(&c);  // a, c, b
    c.add_after_self(&d);  // a, c, d, b
    b.add_after_self(&e);  // a, c, d, b, e
    a.unlink();            // e, c, d, b     (remember: circular)
    int i = 1;
    for (A &it : e) {
        kassert(it.m_x == (i++));
    }
    kassert(i == 5);
    // check that unlinked item is a singular list of itself
    for (A &it : a) {
        i = 1;
        kassert(it.m_x == 7);
    }
    kassert(i == 1);

    TINY_INFO("Pass test_linked_list");
}

static void test_hash_table() {
    ds::hashtable<2> ht;

    kassert(ht.lookup(1337) == nullptr);
    ht.insert(123, (void *)456);
    ht.insert(22, (void *)22);
    ht.insert(13, (void *)37);
    kassert(ht.lookup(13) == (void *)37);
    ht.remove(22);
    kassert(ht.lookup(22) == nullptr);
    kassert(ht.lookup(123) == (void *)456);

    TINY_INFO("Pass test_hash_table");
}

static void test_refcount() {
    struct A : ds::intrusive_refcount {};
    A a {};  // starts with 1 ref
    a.take_ref();
    a.take_ref();
    kassert(a.release_ref() == false);
    a.take_ref();
    kassert(a.release_ref() == false);
    kassert(a.release_ref() == false);
    kassert(a.release_ref() == true);

    TINY_INFO("Pass test_refcount");
}

struct my_node : ds::intrusive_rb_node<my_node> {
    int x;
    inline my_node() : x{0} {}
    inline my_node(int y) : x{y} {}
    int compare(int y) {
        return y - x;
    }
};

// return value: number of black nodes in path to nullptr, must be same for both children
static int printout(ds::intrusive_rb_node<my_node> *node) {
    int left = node->left_node_for_tests() ? static_cast<my_node *>(node->left_node_for_tests())->x : -1;
    int right = node->right_node_for_tests() ? static_cast<my_node *>(node->right_node_for_tests())->x : -1;
    int parent = node->parent_node_for_tests() ? static_cast<my_node *>(node->parent_node_for_tests())->x : -1;
    int x = 0;
    int y = 0;

    TINY_INFO(static_cast<my_node *>(node)->x, ' ', left, ' ', right, " P", parent, ' ', node->is_black_for_test() ? 'B' : 'R');
    if (node->left_node_for_tests()) {
        x = printout(node->left_node_for_tests());
        kassert(node->left_node_for_tests()->parent_node_for_tests() == node);
        if (!node->is_black_for_test())
            kassert(node->left_node_for_tests()->is_black_for_test());
    }
    if (node->right_node_for_tests()) {
        y = printout(node->right_node_for_tests());
        kassert(node->right_node_for_tests()->parent_node_for_tests() == node);
        if (!node->is_black_for_test())
            kassert(node->right_node_for_tests()->is_black_for_test());
    }

    kassert(x == y);
    return x + (node->is_black_for_test() ? 1 : 0);
}

static void test_rbtree() {
    ds::rbtree<my_node> tree;
    // supported ops: find, insert, remove, first, last, next, prev

    // random set of integers test
    int *sorted_arr = reinterpret_cast<int *>(memory::kmem_alloc_4k());
    size_t set_size = 0;
    constexpr size_t set_max_size = 4096 / sizeof(int);

    static_assert(sizeof(my_node) * set_max_size < 32768, "bad size");
    my_node *nodes = reinterpret_cast<my_node *>(memory::kmem_alloc_32k());
    for (size_t i = 0; i < set_max_size; i++) {
        nodes[i].x = i;
    }

    for (int _ = 0; _ < 100; _++) {
    TINY_INFO("start!!!");
    TINY_INFO("start!!!");
    tree.reset();
    set_size = 0;
    for (int i = 0; i < 20; i++) {
        int choice = rng::rand(seed) % 100;
        if (tree.get_root_for_tests() != nullptr) {
            serial_driver::write("\nBBBB\n");
            ds::intrusive_rb_node<my_node> *node = tree.get_root_for_tests();
            printout(node);
            serial_driver::write("\nAAAA\n");
            for (size_t j = 0; j < set_size; j++) {
                serial_driver::write(' ', sorted_arr[j]);
            }
            serial_driver::write('\n');

            // go over all values in order
            TINY_INFO("go over all values");
            node = tree.first();
            for (size_t j = 0; j < set_size; j++) {
                TINY_INFO(static_cast<my_node *>(node)->x, ' ', sorted_arr[j]);
                kassert(static_cast<my_node *>(node)->x == sorted_arr[j]);
                node = node->next_node();
            }
        } else {
            serial_driver::write("\nCCCC\n");
        }
        TINY_INFO(i);
        if (choice < 60) {
            // search a random value and compare to whether it's in sorted_arr
            int value = rng::rand(seed) % set_max_size;
            my_node *node = tree.find(value);

            if (node == nullptr) {
                // make sure it should not exist
                for (size_t j = 0; j < set_size; j++) {
                    kassert(sorted_arr[j] != value)
                }
            } else {
                kassert(node == &nodes[value]);
                // make sure it should exist
                bool found = false;
                for (size_t j = 0; j < set_size; j++) {
                    if (sorted_arr[j] == value) {
                        found = true;
                        break;
                    }
                }
                kassert(found);
            }
        } else if (choice < 70) {
            /*
            // go over all values in order
            TINY_INFO("go over all values");
            my_node *node = static_cast<my_node *>(tree.first());
            for (size_t j = 0; j < set_size; j++) {
                TINY_INFO(node->x, ' ', sorted_arr[j]);
                kassert(node->x == sorted_arr[j]);
                node = static_cast<my_node *>(node->next_node());
            }
            */
        } else if (choice < 80 && set_size != 0) {
            // remove random value from set
            int index = rng::rand(seed) % set_size;
            int value = sorted_arr[index];
            TINY_INFO("remove random value ", value);
            my_node *node = tree.find(value);
            kassert(node == &nodes[value]);
            tree.remove(node);

            set_size--;

            // remove from sorted array
            for (size_t j = index; j < set_size; j++) {
                sorted_arr[j] = sorted_arr[j + 1];
            }
        } else if (choice < 100 && set_size != set_max_size) {
            // insert
            int value = rng::rand(seed) % set_max_size;
            bool inserted = tree.insert(value, &nodes[value]);
            TINY_INFO("insert ", value);
            size_t j = 0;
            for (; j < set_size; j++) {
                if (sorted_arr[j] >= value) {
                    break;
                }
            }
            if (j == set_size || sorted_arr[j] != value) {
                // not found
                kassert(inserted);
                for (size_t k = set_size; k > j; k--) {
                    sorted_arr[k] = sorted_arr[k - 1];
                }
                sorted_arr[j] = value;
                set_size++;
            } else {
                // found
                kassert(!inserted);
            }
        }
    }
    }
}

extern "C" void kmain(multiboot_info_t *multiboot_data, uint multiboot_magic) {
    seed = rng::compile_time_seed();
    serial::initialize();

    memory::read_multiboot_data(multiboot_data, multiboot_magic);
    memory::init_gdt();
    memory::init_page_allocator();

    interrupts::initialize();
    interrupts::init_pic();
    interrupts::start();

    test_bitset();
    test_linked_list();
    test_hash_table();
    test_refcount();
    test_rbtree();

    // test done
    interrupts::cli();
    serial_driver::write("TEST_SUCCESS");
    while (1) { asm volatile("hlt"); }
}
