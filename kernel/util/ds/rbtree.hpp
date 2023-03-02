#pragma once
#include <kernel/util.hpp>
#include <kernel/logging.hpp>
#include <kernel/util/lock.hpp>

namespace ds {
    template <class T>
    struct intrusive_rb_node;
    template <class T>
    struct rbtree;

    template <class T>
    struct rbtree {
        friend intrusive_rb_node<T>;
    private:
        intrusive_rb_node<T> *m_root;
    public:
        template <class K>
        T *find(K key);

        template <class K>
        // returns false if already exists, true if inserted
        bool insert(K key, T *item);

        void remove(T *item);

        intrusive_rb_node<T> *first();
        intrusive_rb_node<T> *last();

        inline void reset();
        inline intrusive_rb_node<T> *get_root_for_tests() { return m_root; }
    public:
        inline rbtree() : m_root {nullptr} {}
        // no copy or move
        rbtree(const rbtree<T> &other) = delete;
        rbtree<T> &operator=(const rbtree<T> &other) = delete;
        rbtree(rbtree<T> &&other) = delete;
        rbtree<T> &operator=(rbtree<T> &&other) = delete;
    };

    template <class T>
    struct intrusive_rb_node {
        friend rbtree<T>;
    private:
        intrusive_rb_node<T> *m_parent;       // nullptr for root
        intrusive_rb_node<T> *m_children[2];  // nullptr for invalid - indexed by LEFT, RIGHT
        bool m_is_black;

        static constexpr inline int LEFT = 0;
        static constexpr inline int RIGHT = 1;

        inline intrusive_rb_node<T> *&left() {
            return m_children[LEFT];
        }
        inline intrusive_rb_node<T> *&right() {
            return m_children[RIGHT];
        }
        inline int dir_of_child(intrusive_rb_node<T> *child) {
            kassert(child == left() || child == right());
            return (child == left()) ? LEFT : RIGHT;
        }

    private:
        template <int dir>
        intrusive_rb_node<T> *prev_or_next() {
            // comments assume prev which is left
            intrusive_rb_node<T> *node = this;
            if (node->m_children[dir] != nullptr) {
                // left then right all the way
                node = node->m_children[dir];
                while (node->m_children[1 - dir] != nullptr)
                    node = node->m_children[1 - dir];
                return node;
            } else {
                // left doesn't exist - go up until you were the right subtree
                // to find the largest item you are greater than
                while (node->m_parent != nullptr && node == node->m_parent->m_children[dir]) {
                    node = node->m_parent;
                }

                return node->m_parent;
            }
        }

    public:
        inline intrusive_rb_node<T> *prev_node() {
            return prev_or_next<LEFT>();
        }
        inline intrusive_rb_node<T> *next_node() {
            return prev_or_next<RIGHT>();
        }
        inline intrusive_rb_node<T> *left_node_for_tests() {
            return left();
        }
        inline intrusive_rb_node<T> *right_node_for_tests() {
            return right();
        }
        inline intrusive_rb_node<T> *parent_node_for_tests() {
            return m_parent;
        }
        inline bool is_black_for_test() {
            return m_is_black;
        }

    private:
        static intrusive_rb_node<T> *rotate(intrusive_rb_node<T> *&tree, intrusive_rb_node<T> &parent, int dir) {
            intrusive_rb_node<T> *gparent = parent.m_parent;
            intrusive_rb_node<T> *sibling = parent.m_children[1 - dir];
            kassert(sibling != nullptr);
            intrusive_rb_node<T> *close = sibling->m_children[dir];

            parent.m_children[1 - dir] = close;

            if (close != nullptr)
                close->m_parent = &parent;

            sibling->m_children[dir] = &parent;
            parent.m_parent = sibling;

            sibling->m_parent = gparent;
            if (gparent != nullptr)
                gparent->m_children[gparent->dir_of_child(&parent)] = sibling;
            else
                tree = sibling;

            return sibling;  // new root of subtree
        }

        // Insert node as a child of the given parent at direction
        static void insert(intrusive_rb_node<T> *&treeroot, intrusive_rb_node<T> *node, intrusive_rb_node<T> *parent, int dir) {
            node->m_parent = parent;
            node->left() = nullptr;
            node->right() = nullptr;
            node->m_is_black = false;

            if (parent == nullptr) {
                node->m_is_black = true;
                treeroot = node;
                return;
            }

            parent->m_children[dir] = node;

            intrusive_rb_node<T> *gparent;
            intrusive_rb_node<T> *uncle;

            do {
                if (parent->m_is_black)
                    return;
                gparent = parent->m_parent;
                if (gparent == nullptr) {
                    parent->m_is_black = true;
                    return;
                }

                dir = gparent->dir_of_child(parent);
                uncle = gparent->m_children[1 - dir];

                if (uncle == nullptr || uncle->m_is_black) {
                    if (node == parent->m_children[1 - dir]) {
                        rotate(treeroot, *parent, dir);
                        node = parent;
                        parent = gparent->m_children[dir];
                    }
                    rotate(treeroot, *gparent, 1 - dir);
                    parent->m_is_black = true;
                    gparent->m_is_black = false;
                    return;
                }

                parent->m_is_black = true;
                uncle->m_is_black = true;
                gparent->m_is_black = false;
                node = gparent;
            } while ((parent = node->m_parent) != nullptr);
        }

        // Remove me from tree
        void remove_me(intrusive_rb_node<T> *&treeroot) {
            TINY_INFO(formatting::hex{this}, ' ', formatting::hex{treeroot}, ' ', left() != nullptr, right() != nullptr);
            if (this == treeroot && left() == nullptr && right() == nullptr) {
                TINY_INFO("a");
                // am a lonely root
                treeroot = nullptr;
                return;
            }
            if (left() != nullptr && right() != nullptr) {
                TINY_INFO("b");
                intrusive_rb_node<T> *replacement = prev_node();

                // root should point at replacement
                if (this == treeroot)
                    treeroot = replacement;

                // parent should point at replacement
                if (m_parent) {
                    if (this == m_parent->left())
                        m_parent->left() = replacement;
                    else
                        m_parent->right() = replacement;
                }

                if (replacement->left() != nullptr)
                    replacement->left()->m_parent = this;
                if (replacement->right() != nullptr)
                    replacement->right()->m_parent = this;

                if (this == replacement->m_parent) {
                    // tmps will be copied into this
                    intrusive_rb_node<T> *tmp_left = replacement->left();
                    intrusive_rb_node<T> *tmp_right = replacement->right();
                    bool tmp_is_black = replacement->m_is_black;

                    if (replacement == left()) {
                        replacement->right() = right();
                        replacement->left() = this;
                    } else {
                        replacement->left() = left();
                        replacement->right() = this;
                    }
                    replacement->m_parent = m_parent;
                    replacement->m_is_black = m_is_black;

                    m_parent = replacement;
                    left() = tmp_left;
                    right() = tmp_right;
                    m_is_black = tmp_is_black;
                } else {
                    TINY_INFO("c");
                    if (replacement == replacement->m_parent->left())
                        replacement->m_parent->left() = this;
                    else
                        replacement->m_parent->right() = this;

                    // exchange replacement and my node information
                    intrusive_rb_node<T> *tmp_left = replacement->left();
                    intrusive_rb_node<T> *tmp_right = replacement->right();
                    bool tmp_is_black = replacement->m_is_black;

                    replacement->left() = left();
                    replacement->right() = right();
                    replacement->m_is_black = m_is_black;

                    left() = tmp_left;
                    right() = tmp_right;
                    m_is_black = tmp_is_black;
                }

                if (replacement->left() != nullptr)
                    replacement->left()->m_parent = replacement;
                if (replacement->right() != nullptr)
                    replacement->right()->m_parent = replacement;
            }

            intrusive_rb_node<T> *parent = m_parent;

            TINY_INFO("d");
            if (!m_is_black) {
                kassert(parent != nullptr);
                parent->m_children[parent->dir_of_child(this)] = nullptr;
                return;
            }

            if (left() == nullptr && right() == nullptr) {
                TINY_INFO("f ", m_parent != nullptr);
                intrusive_rb_node<T> *parent;
                intrusive_rb_node<T> *sibling;
                intrusive_rb_node<T> *close;
                intrusive_rb_node<T> *distant;
                intrusive_rb_node<T> *node = this;

                parent = node->m_parent;
                int dir = parent->dir_of_child(this);
                parent->m_children[dir] = nullptr;
                goto skip_dir_of_child;

                do {
                    dir = parent->dir_of_child(this);
skip_dir_of_child:
                    sibling = parent->m_children[1 - dir];
                    TINY_INFO(sibling != nullptr);
                    distant = sibling->m_children[1 - dir];
                    TINY_INFO(distant != nullptr);
                    close = sibling->m_children[dir];
                    TINY_INFO(close != nullptr);

                    if (sibling != nullptr && !sibling->m_is_black) {
                        TINY_INFO("f1");
                        rotate(treeroot, *parent, dir);
                        parent->m_is_black = false;
                        sibling->m_is_black = true;
                        sibling = close;
                        distant = sibling->m_children[1 - dir];
                        if (distant != nullptr && !distant->m_is_black)
                            goto Case_D6;
                        close = sibling->m_children[dir];
                        if (close != nullptr && !close->m_is_black)
                            goto Case_D5;
                        goto Case_D4;
                    }
                    if (distant != nullptr && !distant->m_is_black) {
Case_D6:
                        TINY_INFO("f2");
                        rotate(treeroot, *parent, dir);
                        sibling->m_is_black = parent->m_is_black;
                        parent->m_is_black = true;
                        distant->m_is_black = true;
                        return;
                    }
                    if (close != nullptr && !close->m_is_black) {
Case_D5:
                        TINY_INFO("f3");
                        rotate(treeroot, *sibling, 1 - dir);
                        sibling->m_is_black = false;
                        close->m_is_black = true;
                        distant = sibling;
                        sibling = close;
                        goto Case_D6;
                    }
                    if (!parent->m_is_black) {
Case_D4:
                        TINY_INFO("f4");
                        sibling->m_is_black = false;
                        parent->m_is_black = true;
                        return;
                    }
                    TINY_INFO("f5");
                    // case 1
                    sibling->m_is_black = false;
                    node = parent;
                } while ((parent = node->m_parent) != nullptr);
            } else {
                TINY_INFO("g ", parent != nullptr);
                // have one child and am black
                intrusive_rb_node<T> *replacement;
                if (left() != nullptr) replacement = left();
                else                   replacement = right();
                replacement->m_parent = parent;
                replacement->m_is_black = true;
                if (parent != nullptr)
                    parent->m_children[parent->dir_of_child(this)] = replacement;
                else
                    treeroot = replacement;
            }
        }

    public:
        inline intrusive_rb_node() {}
        // no copy or move
        intrusive_rb_node(const intrusive_rb_node<T> &other) = delete;
        intrusive_rb_node<T> &operator=(const intrusive_rb_node<T> &other) = delete;
        intrusive_rb_node(intrusive_rb_node<T> &&other) = delete;
        intrusive_rb_node<T> &operator=(intrusive_rb_node<T> &&other) = delete;
    };

    template <class T>
    template <class K>
    T *rbtree<T>::find(K key) {
        intrusive_rb_node<T> *node = m_root;

        while (node != nullptr) {
            T *tnode = static_cast<T *>(node);
            auto compare_result = tnode->compare(key);
            if (compare_result < 0) {
                node = node->left();
            } else if (compare_result > 0) {
                node = node->right();
            } else {
                return tnode;
            }
        }

        // not found
        return nullptr;
    }

    template <class T>
    template <class K>
    bool rbtree<T>::insert(K key, T *item) {
        intrusive_rb_node<T> *parent = nullptr;
        intrusive_rb_node<T> *node = m_root;
        auto dir = intrusive_rb_node<T>::LEFT;

        while (node != nullptr) {
            T *tnode = static_cast<T *>(node);
            parent = node;

            auto compare_result = tnode->compare(key);
            if (compare_result < 0) {
                node = node->left();
                dir = intrusive_rb_node<T>::LEFT;
            } else if (compare_result > 0) {
                node = node->right();
                dir = intrusive_rb_node<T>::RIGHT;
            } else {
                return false;
            }
        }

        intrusive_rb_node<T>::insert(m_root, item, parent, dir);
        return true;
    }

    template <class T>
    void rbtree<T>::remove(T *item) {
        item->remove_me(m_root);
    }

    template <class T>
    intrusive_rb_node<T> *rbtree<T>::first() {
        intrusive_rb_node<T> *node = m_root;
        if (node == nullptr) return nullptr;
        while (node->left() != nullptr)
            node = node->left();
        return node;
    }

    template <class T>
    intrusive_rb_node<T> *rbtree<T>::last() {
        intrusive_rb_node<T> *node = m_root;
        if (node == nullptr) return nullptr;
        while (node->right() != nullptr)
            node = node->right();
        return node;
    }

    template <class T>
    void rbtree<T>::reset() {
        m_root = nullptr;
    }
}
