#include "./insert.h"

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "./btree.h"
#include "./btree_node.h"
#include "./btree_settings.h"
#include "./search.h"

typedef struct ChildIdxCache
{
    int* data;
    int size;
} ChildIdxCache;

static bool update_child_hint_cache(ChildIdxCache* cache, int idx, int val)
{
    if (idx >= cache->size)
    {
        cache->size *= 2;

        int* new_data = (int*)realloc(cache->data, cache->size * sizeof(int));
        if (!new_data)
        {
            return false;
        }

        cache->data = new_data;
    }

    cache->data[idx] = val;

    return true;
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
    int** child_idx_cache_ptr)
{
    ChildIdxCache child_idx_cache = {
        .data = (int*)calloc(DEFAULT_CHILD_IDX_CACHE_SIZE, sizeof(int)),
        .size = DEFAULT_CHILD_IDX_CACHE_SIZE};

    // Depth in the subtree rooted at last_nonfull_anc. Assume a depth of 1 in
    // case all ancestors are full. Otherwise, depth will be reset to zero.
    int depth = 1;
    // Always starts at zero because a new root is never split
    int child_idx = 0;
    update_child_hint_cache(&child_idx_cache, 0, 0);
    // We expect the caller to ensure the tree doesn't contain this key, but may
    // handle this case better in the future
    bool found_key              = 0;
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
        child_idx = compute_child_idx(ptr, key, &found_key);

        if (found_key)
        {
            return 0;
        }

        // Add child_idx to cache
        if (!update_child_hint_cache(&child_idx_cache, depth,
                child_idx_after_split(ptr, key, child_idx)))
        {
            // OOM
            return 0;
        }

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
    *child_idx_cache_ptr  = child_idx_cache.data;

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
    const int size = node->node_size;
    // size of left sibling after the split
    const int new_left_size = size / 2;

    BTreeNode* rsib;
    if (!btree_node_init(size, &rsib, node->children != NULL))
    {
        // OOM
        return 0;
    }

    *rsib_ptr       = rsib;
    *next_key_ptr   = btree_node_get_key(node, new_left_size);

    node->curr_size = new_left_size;

    btree_node_append_key_range(
        rsib, node, new_left_size + 1, size - new_left_size - 1);
    rsib->subtree_size += rsib->curr_size;

#ifdef btree_keep_unused_mem_clean
    memset(node->keys + new_left_size, 0, (size - new_left_size) * sizeof(int));
#endif

    if (!btree_node_is_leaf(node))
    {
        btree_node_append_child_range(
            rsib, node, new_left_size + 1, rsib->curr_size + 1);

#ifdef btree_keep_unused_mem_clean
        memset(node->children + new_left_size + 1, 0,
            (size - new_left_size + 1) * sizeof(BTreeNode*));
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