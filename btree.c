#include "./btree.h"

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./btree_node.h"
#include "./btree_settings.h"
#include "./delete.h"
#include "./printutils.h"
#include "./search.h"
#include "./testutils.h"

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

/**
 * Get the greatest key in the tree ordered before `node->keys[key_idx]`
 * assuming `node` is internal
 */
static void btree_node_get_pred(
    BTreeNode* node, int key_idx, BTreeNode** pred_leaf_ptr, int* pred_ptr)
{
    BTreeNode* ptr = btree_node_get_child(node, key_idx);

    while (!btree_node_is_leaf(ptr))
        btree_node_intl_descend(&ptr, ptr->curr_size);

    *pred_leaf_ptr = ptr;
    *pred_ptr      = btree_node_get_key(ptr, ptr->curr_size - 1);
}

/**
 * @brief Rotate right about `pivot_idx`
 *
 * @details
 *
 * @par Assumptions
 */
static void btree_node_rotate_right(
    BTreeNode* lsib, BTreeNode* rsib, BTreeNode* par, int pivot_idx)
{
    // append pivot and `lsib`'s last child to the front of rsib
    // replace pivot with last key of `lsib`
    int lsib_last_val = 0;

    lsib->curr_size -= 1;
    rsib->curr_size += 1;

    btree_node_pop_back_key(lsib, &lsib_last_val);
    btree_node_push_front_key(rsib, btree_node_get_key(par, pivot_idx));

    lsib->subtree_size -= 1;
    rsib->subtree_size += 1;

    if (!btree_node_is_leaf(lsib))
    {
        BTreeNode* lsib_last_child = NULL;

        btree_node_pop_back_child(lsib, &lsib_last_child);
        btree_node_push_front_child(rsib, lsib_last_child);

        lsib->subtree_size -= lsib_last_child->subtree_size;
        rsib->subtree_size += lsib_last_child->subtree_size;
    }

    btree_node_set_key(par, pivot_idx, lsib_last_val);
}

/**
 * @brief Rotate left about `pivot_idx
 *
 * @details First key of `rsib` is moved to `par` to replace the pivot, and the
 * pivot is moved to the end of `lsib`. If `lsib` and `rsib` are internal, the
 * first child of `rsib` is moved to the end of `lsib`.
 *
 * @par Assumptions - `rsib` has at least t children
 *
 * @param lsib
 * @param rsib
 * @param par
 * @param pivot_idx
 */
static void btree_node_rotate_left(
    BTreeNode* lsib, BTreeNode* rsib, BTreeNode* par, int pivot_idx)
{
    // append pivot and `rsib`'s first child to the back of lsib
    // replace pivot with first key of `rsib`
    int rsib_first_key = 0;

    lsib->curr_size += 1;
    rsib->curr_size -= 1;

    btree_node_pop_front_key(rsib, &rsib_first_key);
    btree_node_set_last_key(lsib, btree_node_get_key(par, pivot_idx));

    lsib->subtree_size += 1;
    rsib->subtree_size -= 1;

    if (!btree_node_is_leaf(lsib))
    {
        BTreeNode* rsib_first_child = NULL;

        btree_node_pop_front_child(rsib, &rsib_first_child);
        btree_node_set_last_child(lsib, rsib_first_child);

        lsib->subtree_size += rsib_first_child->subtree_size;
        rsib->subtree_size -= rsib_first_child->subtree_size;
    }

    btree_node_set_key(par, pivot_idx, rsib_first_key);
}

/**
 * @brief Merge `lsib` and `rsib`, storing the result in `lsib`
 *
 * @param lsib
 * @param rsib
 * @param par
 * @param sep_idx
 *
 * @par Assumptions
 *   - `lsib` and `rsib` are either both leaves or both internal
 */
static void btree_node_merge_sibs(
    BTreeNode* lsib, BTreeNode* rsib, BTreeNode* par, int sep_idx)
{
    // the pivot key, then `rsib`'s keys and (if `lsib` is internal) `rsib`'s
    // children are appended to `lsib`
    if (!btree_node_is_leaf(lsib))
    {
        for (int i = 0; i <= rsib->curr_size; i++)
        {
            btree_node_set_child(
                lsib, lsib->curr_size + 1 + i, btree_node_get_child(rsib, i));
        }
    }

    // key merge
    btree_node_set_key(lsib, lsib->curr_size, btree_node_get_key(par, sep_idx));
    memcpy(lsib->keys + lsib->curr_size + 1, rsib->keys,
        rsib->curr_size * sizeof(int));

    lsib->curr_size += rsib->curr_size + 1;
    lsib->subtree_size += rsib->subtree_size + 1;

    btree_node_kill(rsib);

    // Parent loses the pivot key, but its subtree size doens't change
    btree_node_remove_key(par, sep_idx);
    btree_node_remove_child(par, sep_idx + 1);

    par->curr_size -= 1;
}

/**
 * @brief Deletes a `val` from the btree if the tree contains `val`
 *
 * @details If `val` in a leaf, it is replaced by its predecessor
 *
 * @param[in] root Root of a btree
 * @param[in] val Key to be removed from the tree
 * @param[out] new_root_ptr Pointer to the root of the tree after `val` is
 * (maybe) removed
 *
 * @return A return code
 *    - 0: Error
 *    - 1: OK
 *    - 2: Not found
 */
int btree_node_delete_impl(BTreeNode* root, int val, BTreeNode** new_root_ptr)
{
    return btree_node_delete_impl_2(root, val, new_root_ptr);

    // Allows us to skip a more complicated algorithm if we find it.
    if (!btree_node_contains_key(root, val)) return 2;

    // Default: root doesn't change
    *new_root_ptr                       = root;

    BTreeNode* closest_over_min_cap_anc = NULL;
    BTreeNodeSib* merge_hint_cache      = UNDEFINED;
    BTreeNodeSib* child_idx_cache       = UNDEFINED;
    int last_something_idx_cant_remember;

    if (!btree_node_find_closest_over_min_cap_anc(root, val,
            closest_over_min_cap_anc, &last_something_idx_cant_remember,
            &merge_hint_cache, &child_idx_cache))
        return 0;

    // TODO: Currently, find_key doesn't find the leaf we're looking for. We
    // need a separate function that computes num_min_cap_ancestors for the
    // node containing the predecessor to the key we're deleting.
    BTreeNode* ptr                   = NULL;
    int key_idx                      = 0;
    BTreeNode* last_over_min_cap_anc = NULL;
    int num_min_cap_ancestors = 0, computed_num_min_cap_ancestors = 0;
    int inc_subtree_sizes = 0;
    if (!btree_node_find_key(root, val, &ptr, &key_idx, &last_over_min_cap_anc,
            &num_min_cap_ancestors, inc_subtree_sizes))
        return 0;

    if (!btree_node_is_leaf(ptr))
    {
        // if internal, replace with predecessor and delete predecessor from its
        // leaf node
        BTreeNode* intl = ptr;
        int pred        = 0;
        btree_node_get_pred(intl, key_idx, &ptr, &pred);

        btree_node_set_key(intl, key_idx, pred);
        key_idx = ptr->curr_size - 1;
    }

    // Now `ptr` is definitely a leaf. Delete the value at `key_idx` from the
    // leaf
    btree_node_remove_key(ptr, key_idx);

    ptr->curr_size -= 1;

    // Used for rebalancing
    BTreeNode* sib2 = ptr;

    // Correct subtree size for all affected nodes
    while (ptr != NULL)
    {
        ptr->subtree_size -= 1;
        ptr = ptr->par;
    }

    // Rebalance the tree if the leaf has too few keys
    while (sib2->par != NULL && btree_node_under_min_cap(sib2))
    {
        // sib2 has too few keys -- we can borrow a key from a sibling or merge
        // with a sibling
        BTreeNode* sib1 = NULL;
        BTreeNode* sib3 = NULL;
        BTreeNode* par  = sib2->par;
        btree_node_get_sibs(sib2, sib2->child_idx, &sib1, &sib3);

        if (sib3 != NULL && btree_node_over_min_cap(sib3))
        {
            // right sibling can spare a key
            btree_node_rotate_left(sib2, sib3, par, sib2->child_idx);
        }
        else if (sib1 != NULL && btree_node_over_min_cap(sib1))
        {
            // left sibling can spare a key
            btree_node_rotate_right(sib1, sib2, par, sib2->child_idx - 1);
        }
        else
        {
            // neither sibling can spare a key
            int sep_idx = sib2->child_idx;
            // pick a sibling to merge with
            BTreeNode* lsib = NULL;
            BTreeNode* rsib = NULL;
            if (sib1 != NULL)
            {
                lsib = sib1;
                rsib = sib2;
                sep_idx -= 1;
            }
            else
            {
                lsib = sib2;
                rsib = sib3;
            }

            btree_node_merge_sibs(lsib, rsib, par, sep_idx);
        }

        sib2 = par;
    }

    // If this is the root, it is internal and it has only one child, then we
    // delete it
    if (btree_node_is_root(sib2) && btree_node_is_empty(sib2) &&
        !btree_node_is_leaf(sib2))
    {
        *new_root_ptr = btree_node_get_child(sib2, 0);
        btree_node_kill(sib2);
    }

    return 1;
}