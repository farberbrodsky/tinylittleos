#pragma once
#include <kernel/util.hpp>

namespace ds {
    // circular doubly linked list
    template <class T>
    struct intrusive_doubly_linked_node {
    private:
        intrusive_doubly_linked_node<T> *l_prev;
        intrusive_doubly_linked_node<T> *l_next;

    public:
        inline constexpr intrusive_doubly_linked_node() : l_prev(this), l_next(this) {}
        // no copy or move
        intrusive_doubly_linked_node(const intrusive_doubly_linked_node &other) = delete;
        intrusive_doubly_linked_node &operator=(const intrusive_doubly_linked_node &other) = delete;
        intrusive_doubly_linked_node(intrusive_doubly_linked_node &&other) = delete;
        intrusive_doubly_linked_node &operator=(intrusive_doubly_linked_node &&other) = delete;

        inline T *get_next() {
            return static_cast<T *>(l_next);
        }
        inline T *get_prev() {
            return static_cast<T *>(l_prev);
        }

        inline bool lonely() {
            return l_next == this;
        }

        inline void unlink() {
            l_prev->l_next = l_next;
            l_next->l_prev = l_prev;
            l_prev = this;
            l_next = this;
        }

        inline void add_after_self(T *t) {
            auto *n = static_cast<intrusive_doubly_linked_node<T> *>(t);
            n->l_next = l_next;
            n->l_prev = this;
            l_next->l_prev = n;
            l_next = n;
        }

        struct iterator {
        private:
            bool first_time;
            intrusive_doubly_linked_node<T> *node;

        public:
            inline iterator(intrusive_doubly_linked_node<T> *n, bool b) : first_time(b), node(n) {}

            // prefix increment
            inline iterator &operator++() {
                node = node->l_next;
                first_time = false;
                return *this;
            }
            inline bool operator!=(const iterator &other) {
                return (node != other.node) || (first_time != other.first_time);
            }
            inline T &operator*() {
                return *static_cast<T *>(node);
            }
        };

        inline iterator begin() {
            return iterator{this, true};
        }
        inline iterator end() {
            return iterator{this, false};
        }
    };
}
