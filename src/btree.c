#include "./btree.h"

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../test/testutils.h"
#include "./btree_node.h"
#include "./btree_settings.h"
#include "./delete.h"
#include "./search.h"

/// Formal-ish Definition of a BTree
///
/// Notes on terminology:
///
///     - "vertex" and "node" are used interchangeably
///
/// A btree of order k is a directed tree whose vertices satisfy the following
/// properties:
///
///     0.0. A vertex can be a "root", a "leaf" or an "internal node". There
///          are four types of verticies in a btree: root + leaf,
///          root + internal, leaf, and internal.
///     0.1. Each vertex has a list of increasing integers (`keys`, or
///          "it's keys" informally). A vertex's "size" is the length of this
///          list.
///     0.2. A vertex's children are totally ordered
///
///     1. There is exactly one root in the tree
///     2. All leaves are on the same level (same distance from the root)
///     3. A leaf has no children
///     4. A vertex has at most k children
///     5. The number of keys must be one less than the number of children for
///        an internal node
///     6. An internal root has at least two children
///     7. An internal node that is not a root has at least t = ceil(k / 2)
///        children
///     8. The values of the keys of the i-th child of an internal node must be
///        strictly greater than its (i - 1)-th key.
///     9. The values of the keys of the i-th child of an internal node must be
///        strictly less than its i-th key.
///    10. A leaf node has at least ceil(k / 2) - 1 keys.
///
/// These are the commandments of the btree (technically this is Donald Knuth's
/// definition). btree operations (function calls) must preserve these
/// invariants.
///
/// Implementation Details
///
/// TODO
///
/// Naming Conventions
///
/// 1. If it's for the entire tree rooted at the node, prefix with btree_...
/// 2. If it's just for a btree node, it's btree_node_...
/// 3. If it's just for a btree node, AND that node is a leaf, prefix with
///    btree_node_leaf_...
/// 4. If it's just for a btree node, AND that node is a internal, prefix with
///    btree_node_intl_...

const static int default_child_idx_cache_size = 8;

bool try_grow_cache(void** data_ptr, int* size, size_t elem_size)
{
    // If you hit this, then you REALLY screwed up
    if (*size > INT_MAX / 2)
    {
        return false;
    }

    int new_size       = (*size) * 2;
    void* new_data_ptr = realloc(*data_ptr, new_size * elem_size);

    if (!new_data_ptr)
    {
        return false;
    }

    *data_ptr = new_data_ptr;
    *size     = new_size;

    return true;
}

#define TRY_GROW_CACHE_IMPL(ptr, size) \
    try_grow_cache((void**)&(ptr), (size), sizeof(*(ptr)))

#define TRY_GROW_CACHE(cache) (TRY_GROW_CACHE_IMPL(cache->ptr, cache->size))
////////////////////////////////////////////////////////////////////////////////
// GENERAL                                                                    //
////////////////////////////////////////////////////////////////////////////////

/**
 * @brief Recursively frees all memory referenced by this node assuming all
 * children are uninitialized
 *
 * TODO: This should't be recursive
 *
 * @param node
 */
void btree_subtree_kill(BTreeNode* node)
{
    if (node == NULL) return;

    if (node->children)
    {
        for (int i = 0; i <= node->curr_size; i++)
            btree_subtree_kill(btree_node_get_child(node, i));
    }

    btree_node_kill(node);
}

////////////////////////////////////////////////////////////////////////////////
// INSERTION                                                                  //
////////////////////////////////////////////////////////////////////////////////

/** @brief Determine whether a descendent of `root` contains the key `key`
 *
 *  @param[in] root the root of a btree
 *  @param[in] key the key we're searching for
 *
 *  @return a return code
 *     - 0: tree doesn't contain `key`
 *     - 1: tree contains `key`
 */
int btree_node_contains_key(BTreeNode* root, int key)
{
    BTreeNode* ptr = root;
    int child_idx  = 0;

    // Search for a node containing `key`
    while (!btree_node_is_leaf(ptr))
    {
        if (btree_node_get_key(ptr, ptr->curr_size - 1) < key)
        {
            child_idx = ptr->curr_size;
        }
        else if (btree_node_get_key(ptr, ptr->curr_size - 1) == key)
        {
            return 1;
        }
        else if (btree_node_get_key(ptr, 0) > key)
        {
            child_idx = 0;
        }
        else
        {
            child_idx = binary_search(ptr->keys, 0, ptr->curr_size, key);
            if (btree_node_get_key(ptr, child_idx) == key) return 1;
            child_idx += 1;
        }

        btree_node_intl_descend(&ptr, child_idx);
    }

    // Check if the leaf contains `key`
    if (btree_node_get_key(
            ptr, binary_search(ptr->keys, 0, ptr->curr_size, key)) == key)
    {
        return 1;
    }

    return 0;
}

// Helper for btree_node_find_closest_nonfull_anc and
// btree_node_find_closest_over_min_cap_anc Add child_idx to
// local_child_idx_cache, resizing if necessary. Return the current pointer to
// the cache.
static int update_child_idx_cache(int** child_idx_cache,
    int* child_idx_cache_size,
    const int child_idx,
    const int depth)
{
    // Deref
    int* local_child_idx_cache     = *child_idx_cache;
    int local_child_idx_cache_size = *child_idx_cache_size;

    // Update cache
    if (depth >= local_child_idx_cache_size)
    {
        // Resize
        local_child_idx_cache_size *= 2;
        int* new_local_child_idx_cache = (int*)realloc(
            local_child_idx_cache, sizeof(int) * (local_child_idx_cache_size));

        local_child_idx_cache = new_local_child_idx_cache;
        if (!local_child_idx_cache) return 0;
    }

    local_child_idx_cache[depth] = child_idx;

    // Reref
    *child_idx_cache      = local_child_idx_cache;
    *child_idx_cache_size = local_child_idx_cache_size;

    return 1;
}

// Child index will depend on which half of `ptr` the next ancestor belongs to
static int child_idx_after_split(
    const BTreeNode* ptr, const int key, const int child_idx)
{
    const int mid = ptr->node_size / 2;
    if (btree_node_is_full(ptr) && key > btree_node_get_key(ptr, mid))
        return child_idx - mid - 1;

    return child_idx;
}

static void update_subtree_sizes_upwards(
    BTreeNode* node, const BTreeNode* stop_at, const int inc)
{
    while (node != NULL && node != stop_at)
    {
        node->subtree_size += inc;
        node = node->par;
    }
}

static int compute_child_idx(BTreeNode* node, int key, int* found_key)
{
    int child_idx = 0;
    *found_key    = 0;

    if (btree_node_get_key(node, node->curr_size - 1) < key)
    {
        child_idx = node->curr_size;
    }
#if REDUNDANT < 1
    else if (btree_node_get_key(node, node->curr_size - 1) == key)
    {
        *found_key = 1;
    }
#endif
    else if (btree_node_get_key(node, 0) > key)
    {
        child_idx = 0;
    }
    else
    {
        child_idx = binary_search(node->keys, 0, node->curr_size, key);
#if REDUNDANT < 1
        if (btree_node_get_key(node, child_idx) == key)
        {
            *found_key = 1;
        }
        else
        {
#endif
            child_idx += 1;
#if REDUNDANT < 1
        }
#endif endregion
    }

    return child_idx;
}

/**
 * @brief Finds the leaf of (the tree rooted at) `root` where `key` should be
 * inserted.
 *
 * @details Exactly one leaf `leaf` has a range containing `key`. This function
 *
 *    1. finds `leaf`and its closest ancestor with less than
 *       `root->node_size + 1` children
 *
 *    2. increments the subtree sizes of all ancestors before and including the
 *       first non-full ancestor of `leaf`
 *
 * @par Assumptions
 *    - The tree rooted at `root` does not contain `key`. We check for this, in
 *    the function, but we that check is inteded as a last resort.
 *
 * @param[in] root root of the tree
 * @param[in] key key we're finding
 * @param[out] last_nonfull_anc_ptr pointer to the last non-full ancestor of
 * `leaf` (including `leaf`) consecutive ancestors starting at `leaf` (including
 * `leaf`) that are full
 * @param[out] child_idx_cache path from last_nonfull_anc (or new root if all
 * ancestors are full) to the leaf where `key` will be inserted.
 *
 * @return a return code
 *    - 0: Error
 *    - 1: (success) Didn't find `key`: `node_ptr` points to the leaf where
 *    `key` should be inserted
 */
int btree_node_find_closest_nonfull_anc(const BTreeNode* root,
    const int key,
    const BTreeNode** last_nonfull_anc_ptr,
    const int** child_idx_cache)
{
    int* local_child_idx_cache =
        (int*)malloc(default_child_idx_cache_size * sizeof(int));
    int child_idx_cache_size = default_child_idx_cache_size;
    // Depth in the subtree rooted at last_nonfull_anc. Assume a depth of 1 in
    // case all ancestors are full. Otherwise, depth will be reset to zero.
    int depth = 1;
    // Always zero because a new root is never split
    int child_idx               = 0;
    local_child_idx_cache[0]    = child_idx;

    BTreeNode* ptr              = root;
    BTreeNode* last_nonfull_anc = NULL;
    while (!btree_node_is_leaf(ptr))
    {
        // If this node isn't full, its ancestors won't be affected by insertion
        // EXCEPT that their subtree sizes will need to be incremented. Do that
        // now.
        if (!btree_node_is_full(ptr))
        {
            update_subtree_sizes_upwards(ptr, last_nonfull_anc, +1);
            last_nonfull_anc = ptr;
            depth            = 0;
        }

        // Find the next ancestor
        int found_key = 0;
        child_idx     = compute_child_idx(ptr, key, &found_key);
        if (found_key) return 0;

        // Add child_idx to cache
        if (!update_child_idx_cache(&local_child_idx_cache,
                &child_idx_cache_size,
                child_idx_after_split(ptr, key, child_idx), depth))
            return 0;

        btree_node_intl_descend(&ptr, child_idx);
        depth += 1;
    }

    // One last time.
    if (!btree_node_is_full(ptr))
    {
        // Update subtree sizes of ancestors between last_nonfull_anc and ptr
        update_subtree_sizes_upwards(ptr, last_nonfull_anc, +1);
        last_nonfull_anc = ptr;
    }

#if REDUNDANT < 1
    if (key ==
        btree_node_get_key(ptr, find_idx_of_min_key_greater_than_val(ptr, key)))
    {
        return 0;
    }
#endif

    *last_nonfull_anc_ptr = last_nonfull_anc;
    *child_idx_cache      = local_child_idx_cache;

    return 1;
}

/**
 * @brief Finds the descendent of `root` and index of `root`'s keys of `key`.
 *
 * @par details
 *
 * @par Assumptions
 *    - A descendent of `root` contains `key`
 *
 * @param[in] root root of the tree
 * @param[in] key key we're finding
 * @param[out] node_ptr pointer to the descendent `node` of `root` containing
 * `key`.
 * Set to NULL if the tree doesn't contain `key`.
 * @param[out] key_idx_ptr pointer to the index of `node` where `key` exists
 * @param[out] last_over_min_cap_anc_ptr pointer to the last ancestor of `node`
 * with
 * more than `t` children
 * @param[out] num_min_cap_ancestors_ptr pointer to an integer that is the
 * number of
 * consecutive ancestors starting at `node` with `t` children
 * @param[in] inc_subtree_sizes flag that determines whether we increase the
 * subtree_size of each ancestor of `node`. Used for insertion operations.
 * WARNING: This should only be set if we're certain that the tree contains
 * `key`. TODO: Replace this with a `flags` field
 *
 * @return a return code
 *    - 0: Error
 *    - 1: Found `key`
 */
int btree_node_find_key(BTreeNode* root,
    int key,
    BTreeNode** node_ptr,
    int* key_idx_ptr,
    BTreeNode** last_over_min_cap_anc_ptr,
    int* num_min_cap_ancestors_ptr,
    int inc_subtree_sizes)
{
    BTreeNode* ptr                   = root;
    BTreeNode* last_over_min_cap_anc = NULL;
    int num_min_cap_ancestors        = 0;
    int child_idx                    = 0;

    // Search for a node containing `key`
    while (!btree_node_is_leaf(ptr))
    {
        if (btree_node_has_min_cap(ptr))
        {
            num_min_cap_ancestors += 1;
        }
        else
        {
            num_min_cap_ancestors = 0;
            last_over_min_cap_anc = ptr;
        }

        if (inc_subtree_sizes)
        {
            ptr->subtree_size += 1;
        }

        if (btree_node_get_key(ptr, ptr->curr_size - 1) < key)
        {
            child_idx = ptr->curr_size;
        }
        else if (btree_node_get_key(ptr, ptr->curr_size - 1) == key)
        {
            child_idx = ptr->curr_size - 1;
            goto success;
        }
        else if (btree_node_get_key(ptr, 0) > key)
        {
            child_idx = 0;
        }
        else
        {
            child_idx = binary_search(ptr->keys, 0, ptr->curr_size, key);
            if (btree_node_get_key(ptr, child_idx) == key) goto success;
            child_idx += 1;
        }

        btree_node_intl_descend(&ptr, child_idx);
    }

    if (btree_node_has_min_cap(ptr))
    {
        num_min_cap_ancestors += 1;
    }
    else
    {
        num_min_cap_ancestors = 0;
        last_over_min_cap_anc = ptr;
    }

    if (inc_subtree_sizes)
    {
        ptr->subtree_size += 1;
    }

    // Make sure the leaf `node` contains key
    child_idx = binary_search(ptr->keys, 0, ptr->curr_size, key);
    if (btree_node_get_key(ptr, child_idx) == key)
    {
        goto success;
    }

key_not_found:
    return 0;

success:
    *node_ptr                  = ptr;
    *key_idx_ptr               = child_idx;
    *num_min_cap_ancestors_ptr = num_min_cap_ancestors;
    *last_over_min_cap_anc_ptr = last_over_min_cap_anc;
    return 1;
}

/**
 * @brief Splits a node (subroutine for `btree_node_insert_impl`)
 *
 * @detals `key` is being inserted into the subtree rooted at `node`. This
 * function splits `node`, storing the right half in the node `rsib_ptr` points
 * to and the separation key in `next_key_ptr`.
 *
 * @par Assumptions
 *    - `node` is full
 *
 * @param[in] node Node being split
 * @param[out] rsib Second (right) node `node` is split into
 * @paran[out] next_key_ptr Points to the key `node` was split at
 * @paran[in] key Key we're inserting into the tree
 *
 * @return A return code
 *    - 0: Error (OOM)
 *    - 1: OK
 */
int btree_node_split(BTreeNode* node,
    const BTreeNode** rsib_ptr,
    int* next_key_ptr,
    const int key)
{
    const int size    = node->node_size;
    const int l_start = size / 2;
    const int r_start = l_start + 1;

    BTreeNode* rsib;
    if (!btree_node_init(size, &rsib, node->children != NULL)) return 0;

    *rsib_ptr          = rsib;
    *next_key_ptr      = btree_node_get_key(node, l_start);

    node->curr_size    = l_start;
    rsib->curr_size    = size - r_start;

    rsib->subtree_size = rsib->curr_size;

    memcpy(rsib->keys, node->keys + r_start, rsib->curr_size * sizeof(int));
#ifdef btree_keep_unused_mem_clean
    memset(node->keys + l_start, 0, (size - l_start) * sizeof(int));
#endif

    if (!btree_node_is_leaf(node))
    {
        for (int i = 0; i <= rsib->curr_size; i++)
        {
            btree_node_set_child(
                rsib, i, btree_node_get_child(node, r_start + i));
            rsib->subtree_size += btree_node_get_child(rsib, i)->subtree_size;
        }
#ifdef btree_keep_unused_mem_clean
        memset(node->children + l_start + 1, 0,
            (size - l_start + 1) * sizeof(BTreeNode*));
#endif
    }

    node->subtree_size -= rsib->subtree_size + 1;  // +1 for separation key

    return 1;
}

/**
 * @brief Inserts a key into a btree
 *
 * @details TODO
 *
 * @par AlgorithmThe insertion algorithm is described below:
 *
 *    1. Find the leaf L whose range contains `key`.
 *    2. Find the first ancestor A of L that is not full, or create one if all
 *       are full.
 *    3. Get child B of A where `key` should be inserted.
 *    4. Split B into (B1, B2) at separation key k.
 *    5. Insert (k, B2) into A.
 *    6. A is set to B1 if k < `key` or B2 otherwise.
 *    7. If A is an internal node, go to line 3.
 *    8. [A is a leaf] Insert `key` into A.
 *
 * Variable names in our implementation reference this algorithm (albeit in
 * lower case).
 *
 * @param[in] root Root of a btree
 * @param[in] val Key to be inserted into the tree
 * @param[out] new_root_ptr Pointer to the root of the tree after `val` is
 * inserted
 *
 * @return A return code
 *    - 0: Error
 *    - 1: OK
 *    - 2: `val` already exists in the subtree with root `root`
 */
int btree_node_insert_impl(
    BTreeNode* root, const int key, const BTreeNode** new_root_ptr)
{
    // By default, the root of the tree doesn't change
    *new_root_ptr = root;

    // Exit early if we find `key` in the tree.
    if (btree_node_contains_key(root, key)) return 2;

    BTreeNode* a         = NULL;
    int* child_idx_cache = NULL;
    if (!btree_node_find_closest_nonfull_anc(root, key, &a, &child_idx_cache))
        return 0;

    // Create a new root if all ancestors of the leaf we're inserting `key` into
    // are full
    if (a == NULL)
    {
        if (!btree_node_init(root->node_size, &a, 1)) return 0;

        btree_node_set_first_child(a, root);
        // +1 because btree_node_find_closest_nonfull_anc didn't increment any
        // subtree sizes as all ancestors were full
        a->subtree_size = root->subtree_size + 1;
        *new_root_ptr   = a;
    }

    // Insertion proceeds top-down from the first non-full ancestor of the leaf
    // where `key` will be inserted.
    int depth = 0;
    while (!btree_node_is_leaf(a))
    {
        // Get child B of A whose range contains `key`
        BTreeNode* b1 = btree_node_get_child(a, child_idx_cache[depth]);
        BTreeNode* b2 = NULL;

        // Split B into B1, B2, k (B1: lchild, B2: rchild, k: sep_key)
        int k = 0;
        if (!btree_node_split(b1, &b2, &k, key)) return 0;

        // Insert (k, B2) into A
        btree_node_insert_key_and_child_assuming_not_full(a, k, b2);

        // Descend
        a = key < k ? b1 : b2;
        a->subtree_size += 1;  // for `key`
        depth += 1;
    }

    btree_node_insert_key_and_child_assuming_not_full(a, key, NULL);

    return 1;
}

////////////////////////////////////////////////////////////////////////////////
// DELETION                                                                   //
////////////////////////////////////////////////////////////////////////////////

// Implemented in "delete.h"
// TODO: This is where the API will be for delete