#ifndef __BTREE_NODE_CORE_H__
#define __BTREE_NODE_CORE_H__

// Dumb c nonsense.
typedef struct BTreeNode BTreeNode;

struct BTreeNode
{
    int node_size;
    int curr_size;
    // [keys[i], vals[i]] is a key-value pair.
    BTreeNode* par;
    int child_idx;  // This being correct is not an invariant for performance
                    // reasons
    BTreeNode** children;
    int* keys;
    int is_leaf;
    int subtree_size;
};

// BTreeNode macros
#define btree_node_is_empty(node) (node->curr_size == 0)

#define btree_node_is_full(node) (node->curr_size == node->node_size)

#define btree_node_t(node) \
    (node->node->size + 1) / 2 - 1((node->node_size + 1) / 2 - 1)

#define btree_node_has_min_cap(node) \
    (node->curr_size == (node->node_size + 1) / 2 - 1)

#define btree_node_under_min_cap(node) \
    (node->curr_size < (node->node_size + 1) / 2 - 1)

#define btree_node_over_min_cap(node) \
    (node->curr_size > (node->node_size + 1) / 2 - 1)

#define btree_node_is_leaf(node) (node->is_leaf)

#define btree_node_is_root(node) (node->par == NULL)

#define btree_node_is_initialized(node) \
    (node != NULL & node->keys != NULL & node->children != NULL)

#define btree_node_has_min_cap(node) \
    (node->curr_size == (node->node_size + 1) / 2 - 1)

#define btree_node_ascend(node) (node = node->par)

// Initializers

int btree_node_init(int size, BTreeNode** node_ptr, int is_intl);

int btree_node_leaf_to_intl(BTreeNode* node);

void btree_node_kill(BTreeNode* node);

// Misc.

void btree_node_get_sibs(const BTreeNode* node,
    int child_idx,
    BTreeNode** lsib_ptr,
    BTreeNode** rsib_ptr);

void btree_node_intl_descend(BTreeNode** node, int idx);

int find_idx_of_min_key_greater_than_val(BTreeNode* node, int val);

// Remove, insert key/child

void btree_node_remove_key(BTreeNode* node, int idx);

void btree_node_remove_child(BTreeNode* node, int idx);

void btree_node_insert_key_and_child_assuming_not_full(
    BTreeNode* node, const int key, BTreeNode* child);

// Accessors

int btree_node_get_key(BTreeNode* node, int idx);

void btree_node_set_key(BTreeNode* node, int idx, int key);

BTreeNode* btree_node_get_child(BTreeNode* node, int idx);

void btree_node_set_child(BTreeNode* node, int idx, BTreeNode* child);

// Shortcut Accessors

BTreeNode* btree_node_get_first_child(BTreeNode* node);

BTreeNode* btree_node_get_last_child(BTreeNode* node);

void btree_node_set_first_child(BTreeNode* node, BTreeNode* child);

void btree_node_set_last_child(BTreeNode* node, BTreeNode* child);

int btree_node_get_first_key(BTreeNode* node);

int btree_node_get_last_key(BTreeNode* node);

void btree_node_set_first_key(BTreeNode* node, int key);

void btree_node_set_last_key(BTreeNode* node, int key);

// Push/pop key/child

void btree_node_push_front_key(BTreeNode* node, int key);

void btree_node_push_front_child(BTreeNode* node, BTreeNode* child);

void btree_node_push_back_key(BTreeNode* node, int key);

void btree_node_push_back_child(BTreeNode* node, BTreeNode* child);

void btree_node_pop_front_key(BTreeNode* node, int* key);

void btree_node_pop_front_child(BTreeNode* node, BTreeNode** child);

void btree_node_pop_back_key(BTreeNode* node, int* key);

void btree_node_pop_back_child(BTreeNode* node, BTreeNode** child);

// Range operations

void btree_node_copy_key_range(
    BTreeNode* to, BTreeNode* from, int to_start, int from_start, int num_keys);

void btree_node_append_key_range(
    BTreeNode* to, BTreeNode* from, int from_start, int num_keys);

void btree_node_copy_child_range(BTreeNode* to,
    BTreeNode* from,
    int to_start,
    int from_start,
    int num_children);

void btree_node_append_child_range(
    BTreeNode* to, BTreeNode* from, int from_start, int num_children);

#endif
