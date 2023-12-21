#pragma once

#include "atomic.h"
#include "shared.h"

namespace Generic {

// ========================= functions ======================

/**
 * hash function defined similar to cpp
 */
template <typename T>
struct HashFunction {
    uint32_t operator()(T v);
};

/**
 * equal function
 */
template <typename T>
struct EqualFunction {
    bool operator()(T v1, T v2);
};

/**
 * compare function, comparing an identifier type I to a data type T
 */
template <typename I, typename T>
struct CompareFunction {
    int32_t operator()(I id, T v);
};

// ========================= classes ======================

/**
 * A generic linked list of any type T with lock LockType and a null value of null_value
 */
template <typename T, typename LockType>
class LinkedList {
   protected:
    LockType guard;
    T null_value;

    class Node {
       public:
        T val;
        Node* next;

        Node(T val) : val(val), next(nullptr) {}
    };

    Node* head;
    Node* tail;

   public:
    LinkedList(T null_value) : guard(),
                               null_value(null_value),
                               head(nullptr),
                               tail(nullptr) {}

    ~LinkedList() {
        clear();
    }

    /**
     * clears the linked list
     */
    void clear() {
        LockGuard{guard};

        while (head != nullptr) {
            Node* next = head->next;
            delete head;
            head = next;
        }
        tail = nullptr;
    }

    /**
     * transfers all the items of this list to another on the left side
     */
    template <typename OtherLockType>
    void transfer_left(LinkedList<T, OtherLockType>& oll) {
        LockGuard{guard};

        if (&oll == this || head == nullptr) {
            return;
        }

        LockGuard{oll.guard};

        tail->next = oll.head;
        oll.head = head;
        if (oll.tail == nullptr) {
            oll.tail = tail;
        }

        head = nullptr;
        tail = nullptr;
    }

    /**
     * adds to the left of the linked list
     */
    void add_left(T val) {
        LockGuard{guard};

        Node* new_head = new Node(val);
        new_head->next = head;
        head = new_head;
        if (tail == nullptr) {
            tail = head;
        }
    }

    /**
     * adds to the right of the linked list
     */
    void add_right(T val) {
        LockGuard{guard};

        Node* new_tail = new Node(val);
        if (tail == nullptr) {
            head = new_tail;
        } else {
            tail->next = new_tail;
        }
        tail = new_tail;
    }

    /**
     * removes from the left of the list
     */
    T remove_left() {
        LockGuard{guard};

        if (head == nullptr) {
            return null_value;
        }

        Node* ret = head;
        head = head->next;
        T val = ret->val;
        delete ret;
        if (head == nullptr) {
            tail = nullptr;
        }
        return val;
    }

    /**
     * finds in the list or returns null value if not existing
     */
    T find(T val) {
        LockGuard{guard};

        Node* curr = head;
        while (curr != nullptr) {
            if (EqualFunction<T>()(curr->val, val)) {
                return curr->val;
            }
            curr = curr->next;
        }

        return nullptr;
    }
};

/**
 * a generic hashmap
 */
template <typename K, typename V, typename LockType>
class FixedHashMap {
    struct FixedHashMapEntry {
        K key;
        V val;
        FixedHashMapEntry* next;

        FixedHashMapEntry(K key, V val) : key(key), val(val) {}
    };

    uint32_t num_buckets;
    uint32_t num_items;
    LockType* bucket_guard;
    FixedHashMapEntry** buckets;
    V null_value;

    uint32_t get_bucket_idx(K key) {
        uint32_t hash = HashFunction<K>()(key);
        return hash % num_buckets;
    }

    FixedHashMapEntry* find_prev(uint32_t bucket_idx, K key) {
        FixedHashMapEntry* prev = nullptr;
        FixedHashMapEntry* curr = buckets[bucket_idx];
        while (curr != nullptr && !EqualFunction<K>()(curr->key, key)) {
            prev = curr;
            curr = curr->next;
        }
        return prev;
    }

   public:
    FixedHashMap(uint32_t num_buckets, V null_value) : num_buckets(num_buckets),
                                                       num_items(0),
                                                       bucket_guard(new LockType[num_buckets]),
                                                       buckets(new FixedHashMapEntry*[num_buckets]),
                                                       null_value(null_value) {
        for (uint32_t i = 0; i < num_buckets; i++) {
            buckets[i] = nullptr;
        }
    }

    void put(K key, V val) {
        uint32_t bucket_idx = get_bucket_idx(key);
        LockGuard{bucket_guard[bucket_idx]};

        FixedHashMapEntry* fmpe_prev = find_prev(bucket_idx, key);

        if (fmpe_prev == nullptr ? buckets[bucket_idx] == nullptr : fmpe_prev->next == nullptr) {
            FixedHashMapEntry* entry = new FixedHashMapEntry(key, val);
            num_items++;
            entry->next = buckets[bucket_idx];
            buckets[bucket_idx] = entry;
        } else {
            FixedHashMapEntry*& entry = fmpe_prev == nullptr ? buckets[bucket_idx] : fmpe_prev->next;
            entry->val = val;
        }
    }

    V get(K key) {
        uint32_t bucket_idx = get_bucket_idx(key);
        LockGuard{bucket_guard[bucket_idx]};

        FixedHashMapEntry* fmpe_prev = find_prev(bucket_idx, key);

        if (fmpe_prev == nullptr ? buckets[bucket_idx] == nullptr : fmpe_prev->next == nullptr) {
            return null_value;
        } else {
            FixedHashMapEntry*& entry = fmpe_prev == nullptr ? buckets[bucket_idx] : fmpe_prev->next;
            return entry->val;
        }
    }

    V remove(K key) {
        uint32_t bucket_idx = get_bucket_idx(key);
        LockGuard{bucket_guard[bucket_idx]};

        FixedHashMapEntry* fmpe_prev = find_prev(bucket_idx, key);

        if (fmpe_prev == nullptr ? buckets[bucket_idx] == nullptr : fmpe_prev->next == nullptr) {
            return null_value;
        } else {
            FixedHashMapEntry*& entry = fmpe_prev == nullptr ? buckets[bucket_idx] : fmpe_prev->next;
            V val = entry->val;
            delete *(&entry);
            entry = entry->next;
            num_items--;
            return val;
        }
    }
};

/**
 * A generic red black tree of type T with lock LockType
 * nulltype is provided in consturcotr
 *
 * adapted from
 * https://web.archive.org/web/20190207151651/http://www.eternallyconfuzzled.com/tuts/datastructures/jsw_tut_rbtree.aspx
 */
template <typename T, typename LockType>
class RBTree {
    static constexpr bool LEFT = 0;
    static constexpr bool RIGHT = 1;
    static constexpr bool BLACK = 0;
    static constexpr bool RED = 1;

    struct RBTreeNode {
        T data;
        bool color;
        RBTreeNode* parent;
        RBTreeNode* children[2];

        RBTreeNode(T data) : data(data),
                             color(RED),
                             parent(nullptr),
                             children() {}

        inline void set_child(bool dir, RBTreeNode* child) {
            children[dir] = child;
            if (child != nullptr) {
                child->parent = this;
            }
        }

        inline RBTreeNode* get_child(bool dir) {
            return children[dir];
        }
    };

    RBTreeNode* root;
    LockType guard;
    T null_value;
    CompareFunction<T, T> comparator;

    inline bool is_red(RBTreeNode* node) {
        return node != nullptr && node->color == RED;
    }

    inline RBTreeNode* rotate(RBTreeNode* around, bool dir, bool og_root_color, bool new_root_color) {
        ASSERT(around != nullptr);
        RBTreeNode* new_root = around->get_child(!dir);
        ASSERT(new_root != nullptr);
        around->set_child(!dir, new_root->get_child(dir));
        new_root->set_child(dir, around);
        around->color = og_root_color;
        new_root->color = new_root_color;
        return new_root;
    }

    inline RBTreeNode* get_next(RBTreeNode* curr) {
        // if we can search down
        if (curr->get_child(RIGHT) != nullptr) {
            RBTreeNode* next = curr->get_child(RIGHT);
            while (next->get_child(LEFT) != nullptr) {
                next = next->get_child(LEFT);
            }
            return next;
        }

        // otherwise we have to go up the tree
        RBTreeNode* next = curr->parent;
        while (next != nullptr && next->get_child(RIGHT) == curr) {
            curr = next;
            next = next->parent;
        }
        return next;
    }

    /**
     * returns the minimum node in a subtree
     */
    inline RBTreeNode* find_min_node(RBTreeNode* subtree_root) {
        RBTreeNode* curr = subtree_root;
        if (curr == nullptr) {
            return nullptr;
        }
        RBTreeNode* min = curr;
        while (min->get_child(LEFT) != nullptr) {
            min = min->get_child(LEFT);
        }
        return min;
    }

    /**
     * searches the subtree using an id_comparator for a node just less than the identifier
     * returns null if there are no nodes in the tree or there is no node smaller
     */
    template <typename I, typename IDComparator>
    RBTreeNode* find_just_less_node(RBTreeNode* subtree_root, I identifier, IDComparator id_comparator) {
        RBTreeNode* small = nullptr;
        RBTreeNode* curr = subtree_root;
        while (curr != nullptr) {
            int32_t comparison = id_comparator(identifier, curr->data);
            if (comparison > 0) {
                small = curr;
            }
            curr = curr->get_child(comparison <= 0 ? LEFT : RIGHT);
        }
        return small;
    }

    /**
     * searches the subtree for a the identifier using an id_comparator
     */
    template <typename I, typename IDComparator>
    RBTreeNode* find_equal_node(RBTreeNode* subtree_root, I identifier, IDComparator id_comparator) {
        RBTreeNode* curr = subtree_root;
        while (curr != nullptr) {
            int32_t comparison = id_comparator(identifier, curr->data);
            if (comparison == 0) {
                return curr;
            }
            curr = curr->get_child(comparison < 0 ? LEFT : RIGHT);
        }
        return nullptr;
    }

   public:
    RBTree(T null_value) : RBTree(null_value, CompareFunction<T, T>()) {}

    RBTree(T null_value, CompareFunction<T, T> comparator) : root(nullptr),
                                                             guard(),
                                                             null_value(null_value),
                                                             comparator(comparator) {}

    /**
     * insert the data into the rbtree
     * returns true on successful insertion, false if the data is already in the tree
     */
    bool insert(T data) {
        LockGuard{guard};

        // edge case for null root
        if (root == nullptr) {
            root = new RBTreeNode(data);
            root->color = BLACK;
            return true;
        }

        // we now guarantee that the root is not null

        /** ALGORITHM
         * traverse tree
         * - if at the bottom, insert the node
         * - cases:
         *      - if two children are red, then we should do a color flip (we want to push black down)
         *          - this could create a red violation that we need to fix
         *      - if two children are black, we are good
         *      - if only one child is red, we will update the relevant state only if needed later
         */

        RBTreeNode dummy_p(null_value);
        dummy_p.color = BLACK;
        dummy_p.set_child(RIGHT, root);

        RBTreeNode* grandparent = nullptr;
        RBTreeNode* parent = &dummy_p;
        RBTreeNode* curr = dummy_p.get_child(RIGHT);

        bool gp_to_p_dir = 0;
        bool p_to_c_dir = 1;

        bool did_insert = false;

        while (true) {
            // at bottom, so insert
            if (curr == nullptr) {
                did_insert = true;
                curr = new RBTreeNode(data);
                parent->set_child(p_to_c_dir, curr);
            }

            // otherwise we are at an internal node
            // if both children are red, then we should color flip (to push black down)
            else if (is_red(curr->get_child(LEFT)) && is_red(curr->get_child(RIGHT))) {
                curr->color = RED;
                curr->children[LEFT]->color = BLACK;
                curr->children[RIGHT]->color = BLACK;
            }

            // Debug::printf("%x %x %x\n", grandparent, parent, curr);

            // we may have a red violation after creating this node, so we need to fix that
            if (is_red(curr) && is_red(parent)) {
                // save the greatgp
                RBTreeNode* greatgp = grandparent->parent;

                // we need to rotate around the grandparent.
                // the grandparent's parent wont be null by now as the parent of the root is black
                bool greatgp_to_gp_dir = greatgp->get_child(RIGHT) == grandparent ? RIGHT : LEFT;

                // zag
                if (gp_to_p_dir != p_to_c_dir) {
                    RBTreeNode* new_parent = rotate(parent, gp_to_p_dir, RED, BLACK);
                    grandparent->set_child(gp_to_p_dir, new_parent);
                }

                // zig
                RBTreeNode* new_grandparent = rotate(grandparent, !gp_to_p_dir, RED, BLACK);

                // grandparent
                greatgp->set_child(greatgp_to_gp_dir, new_grandparent);
            }

            int32_t comparison = comparator(data, curr->data);

            // check if we found the node
            if (comparison == 0) {
                break;
            }

            gp_to_p_dir = p_to_c_dir;
            p_to_c_dir = comparison < 0 ? LEFT : RIGHT;

            grandparent = curr->parent;
            parent = curr;
            curr = curr->get_child(p_to_c_dir);
        }

        // finally save the root
        root = dummy_p.get_child(RIGHT);
        root->parent = nullptr;
        root->color = BLACK;

        return did_insert;
    }

    /**
     * removes some data from the rbtree
     * returns the data that was removed
     */
    template <typename I, typename IDComparator>
    T remove(I identifier, IDComparator id_comparator) {
        LockGuard{guard};

        // edge case for null root
        if (root == nullptr) {
            return null_value;
        }

        // we now guarantee that the root is not null

        /** ALGORITHM
         * traverse tree
         * - we note that removing a red node from the tree is trivial!
         * - thus we want to force the node that is removed (the inorder successor) to be
         *      red, so removing it is trivial
         *      (as the inorder succesor in the subtree must be a leaf node, as if it is not, there is certainly
         *       a node less than it)
         * - cases:
         *      - if curr is red, then we are good. move on
         *      - else
         *          - if sibling is black
         *              - if all 4 children are black, then we can make parent black and
         *                  curr and sibling red, pushing red down
         *              - if zig zag, double rotate
         *              - else single
         *          - if sibling is red, then parent must be black, and we rotate towards curr to push red
         *                  parent down
         * - the main idea here is that we go down the tree and maintain the parent coloring, thus
         *      ensuring now red or black violations
         */

        RBTreeNode dummy_p(null_value);
        dummy_p.color = BLACK;
        dummy_p.set_child(RIGHT, root);

        RBTreeNode* grandparent = nullptr;
        RBTreeNode* parent = nullptr;
        RBTreeNode* curr = &dummy_p;
        RBTreeNode* found = nullptr;

        bool p_to_c_dir = 0;
        bool c_to_n_dir = 1;

        // keep going until we are at a leaf
        while (true) {
            grandparent = curr->parent;
            parent = curr;
            curr = curr->get_child(c_to_n_dir);

            if (curr == nullptr) {
                break;
            }

            p_to_c_dir = c_to_n_dir;

            // check if we found the data
            int32_t comparison = id_comparator(identifier, curr->data);

            // we need to go right even if comparison is equal to find inorder successor
            c_to_n_dir = comparison >= 0;

            if (comparison == 0) {
                found = curr;
            }

            // if we are red, we are done
            if (is_red(curr) || is_red(curr->get_child(c_to_n_dir))) {
                continue;
            }

            // by here, we gaurantee that curr and the relevant child are black

            // now if the other child is red, then we have to rotate
            if (is_red(curr->get_child(!c_to_n_dir))) {
                RBTreeNode* new_parent = rotate(curr, c_to_n_dir, RED, BLACK);
                parent->set_child(p_to_c_dir, new_parent);
                continue;
            }

            // at this point we have to look at the sibling as the child is black
            // so if sibling is null, we can continue
            RBTreeNode* sibling = parent->get_child(!p_to_c_dir);
            if (sibling == nullptr) {
                continue;
            }

            // check if we only need to color flip
            if (!is_red(sibling->get_child(p_to_c_dir)) && !is_red(sibling->get_child(!p_to_c_dir))) {
                parent->color = BLACK;
                sibling->color = RED;
                curr->color = RED;
                continue;
            }

            // if all else fails, we can rotate
            bool gp_to_p_dir = grandparent->get_child(RIGHT) == parent ? RIGHT : LEFT;

            // zag
            if (is_red(sibling->get_child(p_to_c_dir))) {
                RBTreeNode* new_sibling = rotate(sibling, !p_to_c_dir, RED, BLACK);
                parent->set_child(!p_to_c_dir, new_sibling);
            }

            // zig
            RBTreeNode* new_parent = rotate(parent, p_to_c_dir, BLACK, RED);
            grandparent->set_child(gp_to_p_dir, new_parent);

            // fix coloring of the two remaining nodes
            new_parent->get_child(!p_to_c_dir)->color = BLACK;
            new_parent->get_child(p_to_c_dir)->get_child(p_to_c_dir)->color = RED;
        }

        T ret_data = null_value;

        // replace and remove if needed
        if (found != nullptr) {
            ret_data = found->data;
            found->data = parent->data;
            grandparent->set_child(grandparent->get_child(RIGHT) == parent ? RIGHT : LEFT,
                                   parent->get_child(parent->get_child(LEFT) == nullptr ? RIGHT : LEFT));
            delete parent;
        }

        // update root
        root = dummy_p.get_child(RIGHT);
        if (root != nullptr) {
            root->color = BLACK;
            root->parent = nullptr;
        }

        return ret_data;
    }

    T remove(T data) {
        return remove(data, comparator);
    }

    /**
     * searches the rb tree using an id_comparator
     */
    template <typename I, typename IDComparator>
    T search(I identifier, IDComparator id_comparator) {
        LockGuard{guard};

        RBTreeNode* found = find_equal_node(root, identifier, id_comparator);
        if (found == nullptr) {
            return null_value;
        }
        return found->data;
    }

    T search(T data) {
        return search(data, comparator);
    }

    /**
     * searches for the data that comes right before this identifier
     */
    template <typename I, typename IDComparator>
    T search_just_less(I identifier, IDComparator id_comparator) {
        LockGuard{guard};
        RBTreeNode* just_less_node = find_just_less_node(root, identifier, id_comparator);
        if (just_less_node == nullptr) {
            return null_value;
        }
        return just_less_node->data;
    }

    T search_just_less(T data) {
        return search_just_less(data, comparator);
    }

    /**
     * runs the callback on each data from start_identifier, inclusive to end_identifier, inclusive
     * the callback is called like `bool keep_going = callback(T data)`
     */
    template <typename I, typename IDComparator, typename Work>
    void foreach_data(I start_identifier, I end_identifier, IDComparator id_comparator, Work callback) {
        LockGuard{guard};

        RBTreeNode* curr = find_just_less_node(root, start_identifier, id_comparator);
        if (curr == nullptr) {
            curr = find_min_node(root);
        } else {
            curr = get_next(curr);
        }

        bool keep_going = true;
        while (curr != nullptr && id_comparator(end_identifier, curr->data) >= 0 && keep_going) {
            keep_going = callback(curr->data);
            curr = get_next(curr);
        }
    }

    template <typename Work>
    void foreach_data(T start, T end, Work callback) {
        foreach_data(start, end, comparator, callback);
    }

    /**
     * deep copies the tree nodes, calling the copy functor on each value
     */
    template <typename Work>
    RBTree<T, LockType>* deep_copy(Work copy_functor) {
        LockGuard{guard};

        RBTree<T, LockType>* copy = new RBTree<T, LockType>(null_value, comparator);

        RBTreeNode* min = find_min_node(root);
        while (min != nullptr) {
            T data = copy_functor(min->data);
            copy->insert(data);
            min = get_next(min);
        }
        return copy;
    }

    /**
     * clears the tree, calling the delete construcotr on each value
     */
    template <typename Work>
    void clear(Work delete_functor) {
        LockGuard{guard};

        while (root != nullptr) {
            delete_functor(remove(root->data));
        }
    }
};

}  // namespace Generic