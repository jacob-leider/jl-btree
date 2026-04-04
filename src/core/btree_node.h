#ifndef __BTREE_NODE_CORE_H__
#define __BTREE_NODE_CORE_H__

#include <stdbool.h>
#include <stddef.h>

#include "./btree_settings.h"

// Dumb c nonsense.
typedef struct BTreeNode BTreeNode;
typedef int BTreeKey;

struct BTreeNode
{
#ifndef BTREE_NODE_NODE_SIZE
    size_t node_size;
#endif
    // Number of keys in the subtree rooted at this node
    size_t subtree_size;

    // Parent node
    BTreeNode* par;

    // Index of this node in its parent
    size_t child_idx;

    // (children[i], keys[i]) is a key-value pair.
    BTreeKey* keys;
    size_t num_keys;
    BTreeNode** children;
    size_t num_children;

    // Siblings
    BTreeNode* left_sib;
    BTreeNode* right_sib;

    bool is_leaf;
};

// Allow this to be set at compile time
#ifdef BTREE_NODE_NODE_SIZE
#define btree_node_node_size(node) BTREE_NODE_NODE_SIZE
#else
#define btree_node_node_size(node) ((node)->node_size)
#endif

#define btree_node_curr_size(node) ((node)->num_keys)
#define btree_node_num_children(node) ((node)->num_children)
#define btree_node_num_keys(node) ((node)->num_keys)
#define btree_node_subtree_size(node) ((node)->subtree_size)
#define btree_node_par(node) ((node)->par)
#define btree_node_is_leaf(node) ((node)->is_leaf)
#define btree_node_keys(node) ((node)->keys)
#define btree_node_children(node) ((node)->children)
#define btree_node_left_sib(node) ((node)->left_sib)
#define btree_node_right_sib(node) ((node)->right_sib)

#define btree_node_set_curr_size(node, new_curr_size) \
    ((node)->num_keys = new_curr_size)
#define btree_node_set_num_keys(node, new_num_keys) \
    ((node)->num_keys = new_num_keys)
#define btree_node_set_num_children(node, new_num_children) \
    ((node)->num_children = new_num_children)
#define btree_node_set_subtree_size(node, new_subtree_size) \
    ((node)->subtree_size = new_subtree_size)
#define btree_node_set_par(node, new_par) ((node)->par = new_par)
#define btree_node_set_keys(node, new_keys) ((node)->keys = new_keys)
#define btree_node_set_children(node, new_children) \
    ((node)->children = new_children)
#define btree_node_set_left_sib(node, sib) ((node)->left_sib = sib)
#define btree_node_set_right_sib(node, sib) ((node)->right_sib = sib)
#define btree_node_set_is_leaf(node, new_is_leaf) \
    ((node)->is_leaf = new_is_leaf)

// While this looks like overkill, these macros will get compiled down to
// nothing, and make it easier to change the way we implement subtree
// size/current size storage in the future
#define btree_node_inc_subtree_size(node, inc) \
    (btree_node_set_subtree_size(node, btree_node_subtree_size(node) + inc))
#define btree_node_inc_subtree_size_1(node) \
    (btree_node_inc_subtree_size(node, 1))
#define btree_node_dec_subtree_size(node, dec) \
    (btree_node_set_subtree_size(node, btree_node_subtree_size(node) - (dec)))
#define btree_node_dec_subtree_size_1(node) \
    (btree_node_dec_subtree_size(node, 1))
#define btree_node_inc_curr_size(node, inc) \
    (btree_node_set_curr_size(node, btree_node_curr_size(node) + inc))
#define btree_node_dec_curr_size(node, dec) \
    (btree_node_set_curr_size(node, btree_node_curr_size(node) - dec))
#define btree_node_inc_curr_size_1(node) (btree_node_inc_curr_size(node, 1))
#define btree_node_dec_curr_size_1(node) (btree_node_dec_curr_size(node, 1))

#define btree_node_inc_num_keys(node, inc) \
    (btree_node_set_num_keys(node, btree_node_num_keys(node) + inc))
#define btree_node_dec_num_keys(node, dec) \
    (btree_node_set_num_keys(node, btree_node_num_keys(node) - dec))
#define btree_node_inc_num_keys_1(node) (btree_node_inc_num_keys(node, 1))
#define btree_node_dec_num_keys_1(node) (btree_node_dec_num_keys(node, 1))

#define btree_node_inc_num_children(node, inc) \
    (btree_node_set_num_children(node, btree_node_num_children(node) + inc))
#define btree_node_dec_num_children(node, dec) \
    (btree_node_set_num_children(node, btree_node_num_children(node) - dec))
#define btree_node_inc_num_children_1(node) \
    (btree_node_inc_num_children(node, 1))
#define btree_node_dec_num_children_1(node) \
    (btree_node_dec_num_children(node, 1))

// BTreeNode macros
#define btree_node_is_root(node) (btree_node_par(node) == NULL)
#define btree_node_is_empty(node) (btree_node_curr_size(node) == 0)
#define btree_node_is_full(node) \
    (btree_node_curr_size(node) == btree_node_node_size(node))
#define btree_node_has_min_cap(node) \
    (btree_node_curr_size(node) == (btree_node_node_size(node) + 1) / 2 - 1)
#define btree_node_under_min_cap(node) \
    (btree_node_curr_size(node) < (btree_node_node_size(node) + 1) / 2 - 1)
#define btree_node_over_min_cap(node) \
    (btree_node_curr_size(node) > (btree_node_node_size(node) + 1) / 2 - 1)
// Don't rely on this, since implementation may change faster than this macro
#define btree_node_is_initialized(node)             \
    (node != NULL & btree_node_keys(node) != NULL & \
        btree_node_children(node) != NULL)

#define btree_node_ascend(node) (node = btree_node_par(node))

// Initializers

#ifndef BTREE_NODE_NODE_SIZE
#define btree_node_init(size, node_ptr, is_intl) \
    (btree_node_init_impl(size, node_ptr, is_intl))
#else
#define btree_node_init(size, node_ptr, is_intl) \
    (btree_node_init_impl(node_ptr, is_intl))
#endif

bool btree_node_init_impl(
#ifndef BTREE_NODE_NODE_SIZE
    size_t size,
#endif
    BTreeNode** node_ptr,
    bool is_intl);

void btree_node_kill(BTreeNode* node);

// Misc.

void btree_node_get_sibs(const BTreeNode* node,
    size_t child_idx,
    BTreeNode** lsib_ptr,
    BTreeNode** rsib_ptr);

void btree_node_intl_descend(BTreeNode** node, size_t idx);

size_t find_idx_of_min_key_greater_than_val(BTreeNode* node, BTreeKey val);

// Remove, insert key/child

void btree_node_insert_key_and_child_assuming_not_full(
    BTreeNode* node, const BTreeKey key, BTreeNode* child);

// Accessors

// Get key
BTreeKey btree_node_get_key(BTreeNode* node, size_t idx);

BTreeKey btree_node_get_first_key(BTreeNode* node);

BTreeKey btree_node_get_last_key(BTreeNode* node);

// Set key
void btree_node_set_key(BTreeNode* node, size_t idx, BTreeKey key);

void btree_node_set_first_key(BTreeNode* node, BTreeKey key);

void btree_node_set_last_key(BTreeNode* node, BTreeKey key);

// Get child
BTreeNode* btree_node_get_child(BTreeNode* node, size_t idx);

BTreeNode* btree_node_get_first_child(BTreeNode* node);

BTreeNode* btree_node_get_last_child(BTreeNode* node);

// Set child
void btree_node_set_child(BTreeNode* node, size_t idx, BTreeNode* child);

void btree_node_set_first_child(BTreeNode* node, BTreeNode* child);

void btree_node_set_last_child(BTreeNode* node, BTreeNode* child);

// Insert key
void btree_node_insert_key(BTreeNode* node, size_t idx, BTreeKey key);

void btree_node_push_front_key(BTreeNode* node, BTreeKey key);

void btree_node_push_back_key(BTreeNode* node, BTreeKey key);

// Remove key
void btree_node_remove_key(BTreeNode* node, size_t idx, BTreeKey* key_ptr);

void btree_node_pop_front_key(BTreeNode* node, BTreeKey* key_ptr);

void btree_node_pop_back_key(BTreeNode* node, BTreeKey* key_ptr);

// Insert child
void btree_node_insert_child(BTreeNode* node, size_t idx, BTreeNode* child);

void btree_node_push_front_child(BTreeNode* node, BTreeNode* child);

void btree_node_push_back_child(BTreeNode* node, BTreeNode* child);

// Remove child
void btree_node_remove_child(
    BTreeNode* node, size_t idx, BTreeNode** child_ptr);

void btree_node_pop_front_child(BTreeNode* node, BTreeNode** child_ptr);

void btree_node_pop_back_child(BTreeNode* node, BTreeNode** child_ptr);

// Range operations

void btree_node_copy_key_range(BTreeNode* to,
    BTreeNode* from,
    size_t to_start,
    size_t from_start,
    size_t num_keys);

void btree_node_append_key_range(
    BTreeNode* to, BTreeNode* from, size_t from_start, size_t num_keys);

void btree_node_copy_child_range(BTreeNode* to,
    BTreeNode* from,
    size_t to_start,
    size_t from_start,
    size_t num_children);

void btree_node_append_child_range(
    BTreeNode* to, BTreeNode* from, size_t from_start, size_t num_children);

void btree_node_clear_key_range(BTreeNode* node, size_t start, size_t num_keys);

void btree_node_clear_child_range(
    BTreeNode* node, size_t start, size_t num_children);

#endif
