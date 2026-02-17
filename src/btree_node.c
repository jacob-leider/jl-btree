#include "./btree_node.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./btree_settings.h"
#include "./search.h"

/// INITIALIZERS & DESTRUCTORS

// @brief Points `node` to a (pointer to a) btree leaf node of
//
//      node size: `size`,
//      curr size: 0,
//      par: NULL.
//
// @param size: Number of values in the node (= #keys - 1)
// @param node: Pointer to a pointer to a btree node
//
// @return a return code
//    - 0: Error (oom)
//    - 1: OK
int btree_node_leaf_init(
#ifndef BTREE_NODE_NODE_SIZE
    int size,
#endif
    BTreeNode** node_ptr)
{
    *node_ptr = (BTreeNode*)malloc(sizeof(BTreeNode));

    if (!(*node_ptr)) return 0;

    (*node_ptr)->is_leaf = 1;
#ifndef BTREE_NODE_NODE_SIZE
    (*node_ptr)->node_size = size;
#endif
    (*node_ptr)->subtree_size = 0;
    (*node_ptr)->curr_size    = 0;
    (*node_ptr)->keys =
        (int*)malloc(sizeof(int) * btree_node_node_size(*node_ptr));
    (*node_ptr)->children = NULL;

    if (!(*node_ptr)->keys)
    {
        free((*node_ptr));
        return 0;
    }

    return 1;
}

// @brief Points `node` to a (pointer to a) btree node of
//
//      node size: `size`,
//      curr size: 0,
//      par: NULL.
//
// @param size: Number of values in the node (= #keys - 1)
// @param node: Pointer to a pointer to a btree node
//
// @return a return code
//    - 0: Error (oom)
//    - 1: OK
int btree_node_intl_init(
#ifndef BTREE_NODE_NODE_SIZE
    int size,
#endif
    BTreeNode** node_ptr)
{
    *node_ptr = (BTreeNode*)malloc(sizeof(BTreeNode));

    if (!(*node_ptr)) return 0;

    (*node_ptr)->is_leaf      = 0;
    (*node_ptr)->subtree_size = 0;
#ifndef BTREE_NODE_NODE_SIZE
    (*node_ptr)->node_size = size;
#endif
    (*node_ptr)->curr_size = 0;
    (*node_ptr)->keys =
        (int*)malloc(sizeof(int) * btree_node_node_size(*node_ptr));

    if (!(*node_ptr)->keys)
    {
        free((*node_ptr));
        return 0;
    }

    (*node_ptr)->children = (BTreeNode**)malloc(
        sizeof(BTreeNode*) * (btree_node_node_size(*node_ptr) + 1));

    if (!(*node_ptr)->children)
    {
        free((*node_ptr)->keys);
        free((*node_ptr));
        return 0;
    }

    return 1;
}

int btree_node_init(
#ifndef BTREE_NODE_NODE_SIZE
    int size,
#endif
    BTreeNode** node_ptr,
    int is_intl)
{
#ifndef BTREE_NODE_NODE_SIZE
    return is_intl ? btree_node_intl_init(size, node_ptr)
                   : btree_node_leaf_init(size, node_ptr);
#else
    return is_intl ? btree_node_intl_init(node_ptr)
                   : btree_node_leaf_init(node_ptr);
#endif
}

int btree_node_leaf_to_intl(BTreeNode* node)
{
    node->is_leaf  = 0;
    node->children = (BTreeNode*)malloc(
        (btree_node_node_size(node) + 1) * sizeof(BTreeNode*));

    if (node->children == NULL)
    {
        return 0;
    }

    return 1;
}

// @brief Self explanatory
//
// @param node
void btree_node_kill(BTreeNode* node)
{
    if (node)
    {
        if (node->children != NULL)
        {
            free(node->children);
        }
        if (node->keys != NULL)
        {
            free(node->keys);
        }
        free(node);
    }
}

// ACCESSORS (1): btree_node_(get|set)_(key|child)

int btree_node_get_key(BTreeNode* node, int idx) { return node->keys[idx]; }

void btree_node_set_key(BTreeNode* node, int idx, int key)
{
    node->keys[idx] = key;
}

BTreeNode* btree_node_get_child(BTreeNode* node, int idx)
{
    return node->children[idx];
}

void btree_node_set_child(BTreeNode* node, int idx, BTreeNode* child)
{
    node->children[idx] = child;
    if (child != NULL)
    {
        child->child_idx = idx;
    }
}

// ACCESSORS (2): btree_node_(get|set)_(first|last)_(key|child)

BTreeNode* btree_node_get_first_child(BTreeNode* node)
{
    return btree_node_get_child(node, 0);
}

BTreeNode* btree_node_get_last_child(BTreeNode* node)
{
    return btree_node_get_child(node, btree_node_curr_size(node));
}

int btree_node_get_first_key(BTreeNode* node)
{
    return btree_node_get_key(node, 0);
}

int btree_node_get_last_key(BTreeNode* node)
{
    return btree_node_get_key(node, btree_node_curr_size(node) - 1);
}

void btree_node_set_first_key(BTreeNode* node, int key)
{
    btree_node_set_key(node, 0, key);
}

void btree_node_set_last_key(BTreeNode* node, int key)
{
    btree_node_set_key(node, btree_node_curr_size(node) - 1, key);
}

void btree_node_set_first_child(BTreeNode* node, BTreeNode* child)
{
    btree_node_set_child(node, 0, child);
}

void btree_node_set_last_child(BTreeNode* node, BTreeNode* child)
{
    btree_node_set_child(node, btree_node_curr_size(node), child);
}

void btree_node_intl_descend(BTreeNode** node, int idx)
{
    *node = btree_node_get_child(*node, idx);
}

/// MISC (1)

// @brief Get the left and right siblings of `node->children[child_idx]` (or
// NULL if nonexistent) assuming `node` is internal
void btree_node_get_sibs(const BTreeNode* node,
    int child_idx,
    BTreeNode** lsib_ptr,
    BTreeNode** rsib_ptr)
{
    BTreeNode* par = node->par;

    if (child_idx - 1 >= 0)
        *lsib_ptr = btree_node_get_child(par, child_idx - 1);
    if (child_idx + 1 <= par->curr_size)
        *rsib_ptr = btree_node_get_child(par, child_idx + 1);
}

// @brief points `idx_ptr` to the index of the least key ordered after `val`
int find_idx_of_min_key_greater_than_val(BTreeNode* node, int val)
{
    // duh
    if (btree_node_is_empty(node)) return 0;

    int idx = binary_search(node->keys, 0, btree_node_curr_size(node), val);

    if (btree_node_get_key(node, idx) == val) return 2;

    // Step onto first index pointing to a value larger than `val`
    if (btree_node_get_key(node, idx) < val) idx += 1;

    return idx;
}

/// REMOVE, INSERT KEY/CHILD

static void btree_node_shift_children(
    BTreeNode* node, int start, int len, int offset)
{
    if (offset > 0)
    {
        for (int i = len - 1; i >= 0; i--)
            btree_node_set_child(node, i + start + offset,
                btree_node_get_child(node, i + start));
    }
    else if (offset < 0)
    {
        for (int i = 0; i < len; i++)
            btree_node_set_child(node, i + start + offset,
                btree_node_get_child(node, i + start));
    }
}

// Doesn't change subtree_size (caller is responsible)
void btree_node_remove_key(BTreeNode* node, int idx)
{
    if (btree_node_curr_size(node) - 1 > idx)
    {
        memmove(node->keys + idx, node->keys + idx + 1,
            (btree_node_curr_size(node) - 1 - idx) * sizeof(int));
    }
    btree_node_dec_curr_size_1(node);
#ifdef BTREE_KEEP_UNUSED_MEM_CLEAN
    btree_node_set_last_key(node, 0);
#endif
}

// Doesn't change subtree_size (caller is responsible)
void btree_node_remove_child(BTreeNode* node, int idx)
{
    btree_node_shift_children(
        node, idx + 1, btree_node_curr_size(node) - idx + 1, -1);
#ifdef BTREE_KEEP_UNUSED_MEM_CLEAN
    btree_node_set_child(node, btree_node_curr_size(node) + 1, NULL);
#endif
}

// @brief Adds a key and child to an internal node.
//
// @detaisl If not full, inserts `val` into node->keys, shifting anything
// greater than `val` to the right by one, and inserts `key` into node->children
// shifting anything "greater" to the right by one.
//
// @par Assumptions
//    - `node` is an inintialized and valid btree internal node
//    - `node` is not full
//
// @param node
// @param val
//
// @return a return code
//    - 0: Error
//    - 1: OK
//    - 2: Value already in the node
void btree_node_insert_key_and_child_assuming_not_full(
    BTreeNode* node, const int key, BTreeNode* child)
{
    int idx = find_idx_of_min_key_greater_than_val(node, key);

    if (idx != btree_node_curr_size(node))
    {
        memmove(node->keys + idx + 1, node->keys + idx,
            (btree_node_curr_size(node) - idx) * sizeof(int));
    }

    btree_node_inc_curr_size_1(node);
    btree_node_set_key(node, idx, key);

    if (!btree_node_is_leaf(node))
    {
        btree_node_shift_children(
            node, idx + 1, btree_node_curr_size(node) - idx - 1, 1);
        btree_node_set_child(node, idx + 1, child);
    }
}

// PUSH/POP PRIMITIVES: btree_node_(pop|push)_(front|back)_(key|child)
//
//    * Assumes the size has already been adjusted (decreased for pop, increased
//    for push)
//    * Assumes the caller will adjust the subtree_size after calling

// Shift keys right and set first key
void btree_node_push_front_key(BTreeNode* node, int key)
{
    btree_node_inc_curr_size_1(node);
#if REDUNDANT > 1
    if (btree_node_curr_size(node) <= 0) return;
#endif
    memmove(node->keys + 1, node->keys,
        (btree_node_curr_size(node) - 1) * sizeof(int));
    btree_node_set_first_key(node, key);

    btree_node_inc_subtree_size_1(node);
}

// Shift children right and sets first child
void btree_node_push_front_child(BTreeNode* node, BTreeNode* child)
{
    btree_node_shift_children(node, 0, btree_node_curr_size(node), 1);
    btree_node_set_first_child(node, child);

#if REDUNDANT > 1
    if (child != NULL)
    {
#endif
        btree_node_inc_subtree_size(node, btree_node_subtree_size(child));
#if REDUNDANT > 1
    }
#endif
}

// Stores first key in `key` and shifts all other keys left
void btree_node_pop_front_key(BTreeNode* node, int* key)
{
    btree_node_dec_curr_size_1(node);
    *key = btree_node_get_first_key(node);
    if (btree_node_curr_size(node) > 0)
    {
        memmove(node->keys, node->keys + 1,
            btree_node_curr_size(node) * sizeof(int));
    }

    btree_node_dec_subtree_size_1(node);
}

// Stores first child in `child` and shifts all other children left
void btree_node_pop_front_child(BTreeNode* node, BTreeNode** child_ptr)
{
    BTreeNode* child = btree_node_get_first_child(node);
    btree_node_shift_children(node, 1, btree_node_curr_size(node) + 1, -1);
#ifdef BTREE_KEEP_UNUSED_MEM_CLEAN
    btree_node_set_child(node, btree_node_curr_size(node) + 1, NULL);
#endif

#if REDUNDANT > 1
    if (child != NULL)
    {
#endif
        btree_node_dec_subtree_size(node, btree_node_subtree_size(child));
#if REDUNDANT > 1
    }
#endif

    *child_ptr = child;
}

// Sets last key (redundant!)
void btree_node_push_back_key(BTreeNode* node, int key)
{
    btree_node_inc_curr_size_1(node);
    btree_node_set_last_key(node, key);
    btree_node_inc_subtree_size_1(node);
}

// Sets last child (redundant!)
void btree_node_push_back_child(BTreeNode* node, BTreeNode* child)
{
    btree_node_set_last_child(node, child);

#if REDUNDANT > 1
    if (child != NULL)
    {
#endif
        btree_node_inc_subtree_size(node, btree_node_subtree_size(child));
#if REDUNDANT > 1
    }
#endif
}

// Stores last key in `key` and erases it
void btree_node_pop_back_key(BTreeNode* node, int* key)
{
    btree_node_dec_curr_size_1(node);
    *key = btree_node_get_key(node, btree_node_curr_size(node));
#ifdef BTREE_KEEP_UNUSED_MEM_CLEAN
    btree_node_set_key(node, btree_node_curr_size(node), 0);
#endif

    btree_node_dec_subtree_size_1(node);
}

// Stores last child in `child` and erases it
void btree_node_pop_back_child(BTreeNode* node, BTreeNode** child_ptr)
{
    BTreeNode* child =
        btree_node_get_child(node, btree_node_curr_size(node) + 1);
#ifdef BTREE_KEEP_UNUSED_MEM_CLEAN
    btree_node_set_child(node, btree_node_curr_size(node) + 1, NULL);
#endif

#if REDUNDANT > 1
    if (child != NULL)
    {
#endif
        btree_node_dec_subtree_size(node, btree_node_subtree_size(child));
#if REDUNDANT > 1
    }
#endif

    *child_ptr = child;
}

void btree_node_copy_key_range(
    BTreeNode* to, BTreeNode* from, int to_start, int from_start, int num_keys)
{
    memcpy(
        to->keys + to_start, from->keys + from_start, num_keys * sizeof(int));
}

void btree_node_append_key_range(
    BTreeNode* to, BTreeNode* from, int from_start, int num_keys)
{
    btree_node_copy_key_range(
        to, from, btree_node_curr_size(to), from_start, num_keys);

    btree_node_inc_curr_size(to, num_keys);
    btree_node_inc_subtree_size(to, num_keys);
}

void btree_node_copy_child_range(BTreeNode* to,
    BTreeNode* from,
    int to_start,
    int from_start,
    int num_children)
{
    for (int i = 0; i < num_children; i++)
    {
        BTreeNode* child = btree_node_get_child(from, from_start + i);
        btree_node_set_child(to, to_start + i, child);
        btree_node_inc_subtree_size(to, btree_node_subtree_size(child));
    }
}

void btree_node_append_child_range(
    BTreeNode* to, BTreeNode* from, int from_start, int num_children)
{
    for (int i = 0; i < num_children; i++)
    {
        BTreeNode* child = btree_node_get_child(from, from_start + i);
        btree_node_set_child(to, i, child);
        btree_node_inc_subtree_size(to, btree_node_subtree_size(child));
    }
}

void btree_node_clear_key_range(BTreeNode* node, int start, int num_keys)
{
    memset(node->keys + start, 0, num_keys * sizeof(int));
}

void btree_node_clear_child_range(BTreeNode* node, int start, int num_children)
{
    memset(node->children + start, 0, num_children * sizeof(BTreeNode*));
}