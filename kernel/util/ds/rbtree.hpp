#pragma once
#include <kernel/util.hpp>
#include <kernel/logging.hpp>
#include <kernel/util/lock.hpp>

// TODO current implementation is just an unbalanced binary tree; implement a red-black tree
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

    private:
        // Insert node as a child of the given parent at direction
        static void insert(intrusive_rb_node<T> *&treeroot, intrusive_rb_node<T> *node, intrusive_rb_node<T> *parent, int dir) {
            node->m_parent = parent;
            node->left() = nullptr;
            node->right() = nullptr;
            if (parent == nullptr) {
                treeroot = node;
                return;
            }
            parent->m_children[dir] = node;
        }

        // Remove me from tree
        void remove_me(intrusive_rb_node<T> *&treeroot) {
            if (this == treeroot && left() == nullptr && right() == nullptr) {
                // am a lonely root
                treeroot = nullptr;
            } else if (left() == nullptr && right() == nullptr) {
                // am lonely but not root
                m_parent->m_children[m_parent->dir_of_child(this)] = nullptr;
            } else {
                // have at least one child: replace me with predecessor or successor
                int child_dir = (left() == nullptr) ? RIGHT : LEFT;
                intrusive_rb_node<T> *replaced_node = m_children[child_dir];
                kassert(replaced_node != nullptr);
                while (replaced_node->m_children[1 - child_dir] != nullptr)
                    replaced_node = replaced_node->m_children[1 - child_dir];

                // unlink replace_node which is easy since it has at most one child
                kassert(replaced_node != nullptr && replaced_node->m_parent != nullptr);
                intrusive_rb_node<T> *replaced_child = replaced_node->left() ? replaced_node->left() : replaced_node->right();
                intrusive_rb_node<T> *replaced_parent = replaced_node->m_parent;
                replaced_parent->m_children[replaced_parent->dir_of_child(replaced_node)] = replaced_child;
                if (replaced_child)
                    replaced_child->m_parent = replaced_parent;

                // replace me with replaced
                replaced_node->m_parent = m_parent;
                replaced_node->left() = left();
                if (left() != nullptr) left()->m_parent = replaced_node;
                replaced_node->right() = right();
                if (right() != nullptr) right()->m_parent = replaced_node;

                if (m_parent != nullptr)
                    m_parent->m_children[m_parent->dir_of_child(this)] = replaced_node;
                else
                    treeroot = replaced_node;
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
