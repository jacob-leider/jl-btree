#include "./btree_node.h"

#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./btree_key.h"
#include "./btree_settings.h"
#include "./mem.h"
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
bool btree_node_leaf_init(
#ifndef BTREE_NODE_NODE_SIZE
    int size,
#endif
    BTreeNode** node_ptr)
{
    BTreeNode* node = (BTreeNode*)jl_btree_malloc(sizeof(BTreeNode));
    if (node == NULL)
    {
        return false;
    }

    btree_node_set_is_leaf(node, true);
#ifndef BTREE_NODE_NODE_SIZE
    btree_node_set_node_size(node, size);
#endif
    btree_node_set_subtree_size(node, 0);
    btree_node_set_curr_size(node, 0);
    btree_node_set_num_children(node, 0);

    BTreeKey* keys_ptr = (BTreeKey*)jl_btree_malloc(
        sizeof(BTreeKey) * btree_node_node_size(node));
    if (keys_ptr == NULL)
    {
        jl_btree_free(node);
        return 0;
    }

    btree_node_set_keys(node, keys_ptr);

    btree_node_set_left_sib(node, NULL);
    btree_node_set_right_sib(node, NULL);
    btree_node_set_children(node, NULL);

    *node_ptr = node;

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
    BTreeNode* node = (BTreeNode*)jl_btree_malloc(sizeof(BTreeNode));
    if (node == NULL)
    {
        return false;
    }

    btree_node_set_is_leaf(node, false);
#ifndef BTREE_NODE_NODE_SIZE
    btree_node_set_node_size(node, size);
#endif
    btree_node_set_subtree_size(node, 0);
    btree_node_set_curr_size(node, 0);
    btree_node_set_num_children(node, 0);

    BTreeKey* keys_ptr = (BTreeKey*)jl_btree_malloc(
        sizeof(BTreeKey) * btree_node_node_size(node));
    if (keys_ptr == NULL)
    {
        jl_btree_free(node);
        return 0;
    }

    btree_node_set_keys(node, keys_ptr);

    btree_node_set_left_sib(node, NULL);
    btree_node_set_right_sib(node, NULL);

    BTreeNode** children_ptr = (BTreeNode**)jl_btree_malloc(
        sizeof(BTreeNode*) * (btree_node_node_size(node) + 1));
    if (children_ptr == NULL)
    {
        jl_btree_free(node->keys);
        jl_btree_free(node);
        return 0;
    }

    btree_node_set_children(node, children_ptr);

    *node_ptr = node;

    return 1;
}

bool btree_node_init_impl(
#ifndef BTREE_NODE_NODE_SIZE
    size_t size,
#endif
    BTreeNode** node_ptr,
    bool is_intl)
{
#ifndef BTREE_NODE_NODE_SIZE
    return is_intl ? btree_node_intl_init(size, node_ptr)
                   : btree_node_leaf_init(size, node_ptr);
#else
    return is_intl ? btree_node_intl_init(node_ptr)
                   : btree_node_leaf_init(node_ptr);
#endif
}

// @brief Self explanatory
//
// @param node
void btree_node_kill(BTreeNode* node)
{
    if (node != NULL)
    {
        if (btree_node_children(node) != NULL)
        {
            jl_btree_free(btree_node_children(node));
        }

        if (btree_node_keys(node) != NULL)
        {
            jl_btree_free(btree_node_keys(node));
        }

        jl_btree_free(node);
    }
}

/************************************************************/

// Storage routine

bool btree_node_write_key(BTreeNode* node, size_t idx, BTreeKey* key)
{
    assert(idx < btree_node_node_size(node));

    unsigned char* data = key->data;
    unsigned char* new_data =
        (unsigned char*)jl_btree_malloc(key->size * sizeof(unsigned char));

    if (new_data == NULL)
    {
        return false;
    }

    memcpy(new_data, data, key->size * sizeof(unsigned char));

    BTreeKey new_key = {
        .data = new_data,
        .size = key->size,
    };

    // Should work?
    // TODO: Confirm
    // Nope!
    memcpy(btree_node_keys(node) + idx, &new_key, sizeof(BTreeKey));

    return true;
}

/************************************************************/

// ACCESSORS (1): btree_node_(get|set)_(key|child)

static size_t last_key_idx(BTreeNode* node)
{
    return btree_node_num_keys(node) - 1;
}

BTreeKey* btree_node_get_key(BTreeNode* node, size_t idx)
{
    return btree_node_keys(node) + idx;
}

BTreeKey* btree_node_get_first_key(BTreeNode* node)
{
    assert(btree_node_num_keys(node) > 0);

    return btree_node_get_key(node, 0);
}

BTreeKey* btree_node_get_last_key(BTreeNode* node)
{
    assert(btree_node_num_keys(node) > 0);

    return btree_node_get_key(node, last_key_idx(node));
}

void btree_node_set_key(BTreeNode* node, size_t idx, BTreeKey* key)
{
    btree_node_write_key(node, idx, key);
}

void btree_node_set_first_key(BTreeNode* node, BTreeKey* key)
{
    btree_node_set_key(node, 0, key);
}

void btree_node_set_last_key(BTreeNode* node, BTreeKey* key)
{
    btree_node_set_key(node, last_key_idx(node), key);
}

static size_t last_child_idx(BTreeNode* node)
{
    return btree_node_num_children(node) - 1;
}

BTreeNode* btree_node_get_child(BTreeNode* node, size_t idx)
{
    assert(idx < btree_node_num_children(node));

    return btree_node_children(node)[idx];
}

BTreeNode* btree_node_get_first_child(BTreeNode* node)
{
    assert(btree_node_num_children(node) > 0);

    return btree_node_get_child(node, 0);
}

BTreeNode* btree_node_get_last_child(BTreeNode* node)
{
    assert(btree_node_num_children(node) > 0);

    return btree_node_get_child(node, last_child_idx(node));
}

void btree_node_set_child(BTreeNode* node, size_t idx, BTreeNode* child)
{
    assert(idx <= btree_node_node_size(node));

    btree_node_children(node)[idx] = child;
}

void btree_node_set_first_child(BTreeNode* node, BTreeNode* child)
{
    btree_node_set_child(node, 0, child);
}

void btree_node_set_last_child(BTreeNode* node, BTreeNode* child)
{
    btree_node_set_child(node, last_child_idx(node), child);
}

// ACCESSORS (2): btree_node_(get|set)_(first|last)_(key|child)

void btree_node_intl_descend(BTreeNode** node, size_t idx)
{
    assert(node != NULL);
    assert(idx < btree_node_num_children(*node));

    *node = btree_node_get_child(*node, idx);
}

/// MISC (1)

// @brief Get the left and right siblings of
// `btree_node_children(node)[child_idx]` (or NULL if nonexistent) assuming
// `node` is internal
void btree_node_get_sibs(const BTreeNode* node,
    size_t child_idx,
    BTreeNode** lsib_ptr,
    BTreeNode** rsib_ptr)
{
    BTreeNode* par = btree_node_par(node);

    if (child_idx == 0)
    {
        printf("Error. Underflow risk in %s at line %d.\n", __FILE__, __LINE__);
    }

    if (child_idx > 0)
    {
        *lsib_ptr = btree_node_get_child(par, child_idx - 1);
    }

    if (child_idx + 1 <= btree_node_curr_size(par))
    {
        *rsib_ptr = btree_node_get_child(par, child_idx + 1);
    }
}

/**
 * @brief Computes the index of the least key in `node` ordered strictly after
 * `key`, and determine whether `node` contains `key`
 *
 * @param[in] node A btree node
 * @param[in] key A btree key
 * @param[out] found A boolean indicating whether `node` contains `key`
 *
 * @return Index of the least key in `node` ordered strictly after
 * `key`
 */
size_t find_idx_of_min_key_greater_than_val(
    BTreeNode* node, BTreeKey* key, bool* found)
{
    assert(node != NULL);

    size_t num_keys = btree_node_num_keys(node);

    // Default: Key was not found
    *found = false;

    if (btree_node_is_empty(node))
    {
        // Trivial case (1)
        return 0;
    }

    /******************************************************************/
    /*           The range of `node` DOES NOT contain `key`           */
    /******************************************************************/

    BTreeKey* first_key = btree_node_get_first_key(node);
    if (btree_key_lt(key, first_key))
    {
        // Less than minimum
        return 0;
    }

    BTreeKey* last_key = btree_node_get_last_key(node);
    if (btree_key_gt(key, last_key))
    {
        // Greater than maximum
        return num_keys;
    }

    /******************************************************************/
    /*             The range of `node` DOES contain `key`             */
    /******************************************************************/

    // TODO: take a comparator parameter and only use btree_key_cmp if it is
    // null.
    size_t idx =
        binary_search_3(btree_node_keys(node), 0, num_keys, key, btree_key_cmp);

    // ATP, if `key` is equal to `node->keys[i]` for any `i` (including
    // `node->num_keys - 1`), then `idx` is equal to `i`.

    BTreeKey* closest_key = btree_node_get_key(node, idx);
    if (btree_key_eq(key, closest_key))
    {
        // TODO: Do we really want to let callers pass in a null pointer for
        // `found`? Will this make debugging more difficult down the line?
        if (found != NULL)
        {
            *found = true;
        }
    }

    // ATP, the key at index `idx` is guaranteed to be < `key` and the key at
    // index `idx + 1` is guaranteed to be > `key`

    // Step onto first index pointing to a value larger than `val`
    idx += 1;

    return idx;
}

/// PUSH/POP PRIMITIVES: btree_node_(pop|push)_(front|back)_(key|child)

void btree_node_insert_key(BTreeNode* node, size_t idx, BTreeKey* key)
{
    assert(node != NULL);
    assert(btree_node_num_keys(node) < btree_node_node_size(node));
    assert(idx <= btree_node_num_keys(node));

    btree_node_inc_num_keys_1(node);
    if (btree_node_num_keys(node) > 1)
    {
        BTreeKey* temp = NULL;
        for (size_t i = idx; i < last_key_idx(node); i++)
        {
            temp               = btree_node_get_key(node, i);
            BTreeKey* next_key = btree_node_get_key(node, i + 1);

            btree_node_set_key(node, i, next_key);
            btree_node_set_key(node, i + 1, temp);
        }
    }

    btree_node_set_key(node, idx, key);
}

void btree_node_push_front_key(BTreeNode* node, BTreeKey* key)
{
    // Validate: Can't be full already
    btree_node_insert_key(node, 0, key);
}

void btree_node_push_back_key(BTreeNode* node, BTreeKey* key)
{
    // Validate: Can't be full already
    btree_node_insert_key(node, btree_node_num_keys(node), key);
}

// Stores first key in `key` and shifts all other keys left

void btree_node_remove_key(BTreeNode* node, size_t idx, BTreeKey* key_ptr)
{
    // Validate: Can't be empty, idx must be in bounds
    assert(node != NULL);
    assert(btree_node_num_keys(node) > 0);
    assert(idx < btree_node_num_keys(node));

    if (key_ptr != NULL)
    {
        BTreeKey* key_at_idx = btree_node_get_key(node, idx);
        memcpy(key_ptr, key_at_idx, sizeof(BTreeKey));
    }

    for (size_t i = idx + 1; i < btree_node_num_keys(node); i++)
    {
        btree_node_set_key(node, i - 1, btree_node_get_key(node, i));
    }

#ifdef BTREE_KEEP_UNUSED_MEM_CLEAN
    // TODO: Clear last key
#endif

    btree_node_dec_num_keys_1(node);
}

void btree_node_pop_front_key(BTreeNode* node, BTreeKey* key_ptr)
{
    btree_node_remove_key(node, 0, key_ptr);
}

void btree_node_pop_back_key(BTreeNode* node, BTreeKey* key_ptr)
{
    btree_node_remove_key(node, last_key_idx(node), key_ptr);
}

void btree_node_insert_child(BTreeNode* node, size_t idx, BTreeNode* child)
{
    assert(node != NULL);
    assert(idx <= btree_node_num_children(node));
    assert(!btree_node_is_leaf(node));

    btree_node_inc_num_children_1(node);
    for (size_t i = btree_node_num_children(node) - 1; i > idx; i--)
    {
        btree_node_set_child(node, i, btree_node_get_child(node, i - 1));
    }

    btree_node_set_child(node, idx, child);

    // Maintain invariants
    btree_node_set_par(child, node);
    // For now, we allow children to be null.

    if (idx > 0)
    {
        // Siblings
        BTreeNode* old_first_child = btree_node_get_child(node, idx - 1);

        if (old_first_child != NULL)
        {
            btree_node_set_left_sib(old_first_child, child);
        }

        if (child != NULL)
        {
            btree_node_set_right_sib(child, old_first_child);
        }
    }
    if (idx < btree_node_num_children(node) - 1)
    {
        // Siblings
        BTreeNode* old_last_child = btree_node_get_child(node, idx + 1);

        if (old_last_child != NULL)
        {
            btree_node_set_right_sib(old_last_child, child);
        }

        if (child != NULL)
        {
            btree_node_set_left_sib(child, old_last_child);
        }
    }
}

void btree_node_push_front_child(BTreeNode* node, BTreeNode* child)
{
    btree_node_insert_child(node, 0, child);
}

void btree_node_push_back_child(BTreeNode* node, BTreeNode* child)
{
    btree_node_insert_child(node, btree_node_num_children(node), child);
}

// Stores first child in `child` and shifts all other children left

void btree_node_remove_child(BTreeNode* node, size_t idx, BTreeNode** child_ptr)
{
    assert(node != NULL);
    // Underflow risk
    assert(idx < btree_node_num_children(node));

    if (child_ptr != NULL)
    {
        *child_ptr = btree_node_get_child(node, idx);
    }

    for (size_t i = idx; i < btree_node_num_children(node) - 1; i++)
    {
        btree_node_set_child(node, i, btree_node_get_child(node, i + 1));
    }

    // Siblings
    // TODO: Yikes
    BTreeNode* left_sib  = NULL;
    BTreeNode* right_sib = NULL;
    if (idx > 0 && idx < btree_node_num_children(node) - 1)
    {
        left_sib  = btree_node_get_child(node, idx - 1);
        right_sib = btree_node_get_child(node, idx);

        btree_node_set_left_sib(right_sib, left_sib);
        btree_node_set_right_sib(left_sib, right_sib);
    }
    else if (idx > 0)
    {
        left_sib = btree_node_get_child(node, idx - 1);

        btree_node_set_right_sib(left_sib, NULL);
    }
    else if (idx < btree_node_num_children(node) - 1)
    {
        right_sib = btree_node_get_child(node, idx);

        btree_node_set_left_sib(right_sib, NULL);
    }  // Else, node size is 0 (obviously an issue).

#ifdef BTREE_KEEP_UNUSED_MEM_CLEAN
    btree_node_set_child(node, btree_node_curr_size(node) + 1, NULL);
#endif

    btree_node_dec_num_children_1(node);
}

void btree_node_pop_front_child(BTreeNode* node, BTreeNode** child_ptr)
{
    btree_node_remove_child(node, 0, child_ptr);
}

void btree_node_pop_back_child(BTreeNode* node, BTreeNode** child_ptr)
{
    btree_node_remove_child(node, last_child_idx(node), child_ptr);
}

void btree_node_copy_key_range(BTreeNode* to,
    BTreeNode* from,
    size_t to_start,
    size_t from_start,
    size_t num_keys)
{
    memcpy(to->keys + to_start, from->keys + from_start,
        num_keys * sizeof(BTreeKey));
}

void btree_node_append_key_range(
    BTreeNode* to, BTreeNode* from, size_t from_start, size_t num_keys)
{
    assert(from_start + num_keys <= btree_node_num_keys(from));
    assert(num_keys <= btree_node_node_size(to));

    for (size_t i = 0; i < num_keys; i++)
    {
        btree_node_push_back_key(to, btree_node_get_key(from, from_start + i));
    }
}

void btree_node_copy_child_range(BTreeNode* to,
    BTreeNode* from,
    size_t to_start,
    size_t from_start,
    size_t num_children)
{
    for (size_t i = 0; i < num_children; i++)
    {
        BTreeNode* child = btree_node_get_child(from, from_start + i);
        btree_node_set_child(to, to_start + i, child);
    }
}

void btree_node_append_child_range(
    BTreeNode* to, BTreeNode* from, size_t from_start, size_t num_children)
{
    for (size_t i = 0; i < num_children; i++)
    {
        BTreeNode* child = btree_node_get_child(from, from_start + i);
        btree_node_push_back_child(to, child);
    }
}

void btree_node_clear_key_range(BTreeNode* node, size_t start, size_t num_keys)
{
    memset(btree_node_keys(node) + start, 0, num_keys * sizeof(BTreeKey));
}

void btree_node_clear_child_range(
    BTreeNode* node, size_t start, size_t num_children)
{
    memset(btree_node_children(node) + start, 0,
        num_children * sizeof(BTreeNode*));
}