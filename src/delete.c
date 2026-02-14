#include "delete.h"

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "btree.h"
#include "btree_node.h"
#include "btree_settings.h"
#include "printutils.h"
#include "search.h"

typedef struct ChildIdxCache
{
    int* data;
    int size;
} ChildIdxCache;

typedef struct MergeHintCache
{
    BTreeNodeSib* data;
    int size;
} MergeHintCache;

typedef struct BtreeNodeSibCache
{
    bool refreshed;
    BTreeNode* left;
    BTreeNode* right;
} BtreeNodeSibCache;

static int compute_child_idx(BTreeNode* node, int key, bool* found_key)
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
 * @brief Determines whether a node or one of its siblings has more than the
 * minimum number of keys
 *
 * @param node
 * @param child_idx Index `node` `in node->par->children` if `node` is not a
 * root. Ignored otherwise.
 * @param rotate_hint_ptr Pointer to an integer that is set to:
 *    - 0: `node` has more than the minimum number of keys
 *    - -1: left sibling has more than the minimum number of keys
 *    - 1: right sibling has more than the minimum number of keys
 * Ignored if the return value is 0.
 *
 * @return 0 if neither `node` nor its siblings have more than the minimum
 * number of keys, 1 otherwise
 */
static bool btree_node_is_or_has_sib_over_min_cap(
    BTreeNode* node, int child_idx)
{
    if (btree_node_over_min_cap(node))
    {
        return true;
    }

    // TODO: Fix this terminology in function names
    // Root and min capacity => not "over min capacity"
    if (node->par == NULL)
    {
        // Minimum capacity for a root is 1
        return node->curr_size > 1;
    }

    BTreeNode* left  = NULL;
    BTreeNode* right = NULL;
    btree_node_get_sibs(node, child_idx, &left, &right);

    if (left != NULL && btree_node_over_min_cap(left))
    {
        return true;
    }

    if (right != NULL && btree_node_over_min_cap(right))
    {
        return true;
    }

    return false;
}

bool update_child_hint_cache(ChildIdxCache* cache, int idx, int val)
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

bool update_merge_hint_cache(MergeHintCache* cache, int idx, BTreeNodeSib val)
{
    if (idx >= cache->size)
    {
        cache->size *= 2;

        BTreeNodeSib* new_data = (BTreeNodeSib*)realloc(
            cache->data, cache->size * sizeof(BTreeNodeSib));
        if (!new_data)
        {
            return false;
        }

        cache->data = new_data;
    }

    cache->data[idx] = val;

    return true;
}

/**
 * @brief Determine which sibling this (min-cap) node will merge with
 *
 * @details In the future, this may use a nontrivial heuristic to select for
 * efficiency. In the meantime, it checks left, then right, returning the first
 * non-null option.
 *
 * @param node A node at min capacity
 * @param child_idx Index of `node` in its parent
 * @return The sibling `node` should merge with if it needs to be merged during
 * a deletion
 */
BTreeNodeSib compute_merge_hint(BTreeNode* node, int child_idx)
{
    if (btree_node_over_min_cap(node))
    {
        return NEITHER;
    }

    if (node->par == NULL)
    {
        return UNDEFINED;
    }

    BTreeNode* left;
    BTreeNode* right;
    btree_node_get_sibs(node, child_idx, &left, &right);

    if (left != NULL)
    {
        return LEFT;
    }

    if (right != NULL)
    {
        return RIGHT;
    }

    return UNDEFINED;
}

BTreeNodeSib compute_rotate_hint(BTreeNode* node, int child_idx)
{
    if (btree_node_over_min_cap(node))
    {
        return NEITHER;
    }

    if (node->par == NULL)
    {
        return UNDEFINED;
    }

    BTreeNode* left;
    BTreeNode* right;
    btree_node_get_sibs(node, child_idx, &left, &right);

    if (left != NULL)
    {
        return LEFT;
    }

    if (right != NULL)
    {
        return RIGHT;
    }

    return UNDEFINED;
}

/**
 * @brief TODO
 *
 * @par Details
 *
 * @par last_over_min_cap_anc, rotate_hint and subtree_sizes
 *
 * Whenever a node A with
 * a child to spare/a sibling with a child to spare is encountered, it
 * becomes the new candidate for the end of the min-cap chain. If A' is the
 * last node meeting these criteria, the subtree sizes of nodes between A
 * and A' (including A but not A') are incremented. Thus, after encountering
 * the deepest ancestor of the pred-leaf meeting these criteria, each node
 * between the pred leaf and root that is not in the min-cap chain will have
 * had its subtree size incremented exactly once.
 *
 * After the algorithm terminates, the last node we stored satisfying these
 * criteria will be the last node not in the in cap chain. Moreover, last
 * rotate hint will correspond to the deepest ancestor of the pred-leaf not
 * in the min cap chain.
 *
 * @par child_cache
 *
 * For each ancestor A of the pred-leaf, the child cache is flushed if A can
 * spare/borrow a child, and a pointer to its descendent is subsequently
 * stored in the child cache. Thus, the child cache will be the path from
 * the closest ancestor of the pred-leaf not in the min-cap chain to the
 * pred-leaf.
 *
 * @par merge_hint_cache
 *
 * `merge_hint_cache[n]` contains...
 *
 *      (n = 0): Rotate hint,
 *
 *      (n > 0): Merge hint.
 *
 * For each ancestor A of the pred-leaf: If A can spare/borrow, the merge
 * cache is flushed and a rotate hint is added. If A can't spare/borrow, a
 * merge hint is added. Therefore, when the algorithm terminates, the merge
 * cache will contain a rotate hint, followed by one merge hint for each
 * node in the min-cap chain.
 *
 * @par chain_end_child_idx_ptr
 *
 * If last_over_min_cap_anc (denote by A) is not null, we may need to rotate.
 * There are two cases.
 *
 *      1. A is a root. In this case, A must have current size > 1, so the merge
 *         hint will be NEITHER. Thus, this value will be unused. We set it to
 *         -1 in this case to prevent accidental usage.
 *
 *      2. A is not a root. In this case, we may need to rotate, which requires
 *         us to know the child index of A. This is already stored in
 *         last_child_idx`, so we simply store this value.
 *
 * `chain_end_child_idx_ptr` is reset whenever we encounter a node that can
 * spare/borrow. Thus, when the algorithm terminates, if we end up needing to
 * rotate, the value stored in `chain_end_child_idx_ptr` will be the child index
 * of the node we're rotating keys into.
 *
 *
 * TODO: We technically don't need sib_cache. `btree_node_get_sibs` is already a
 * constant time operation. All we're saving is a reference to the parent, then
 * a reference to its array of children.
 *
 *
 * @param ptr [TODO:parameter]
 * @param last_child_idx_ptr [TODO:parameter]
 * @param child_idx [TODO:parameter]
 * @param last_over_min_cap_anc_ptr [TODO:parameter]
 * @param chain_end_child_idx_ptr [TODO:parameter]
 * @param child_cache [TODO:parameter]
 * @param merge_hint_cache [TODO:parameter]
 * @param sib_cache [TODO:parameter]
 * @param depth_ptr [TODO:parameter]
 * @return [TODO:return]
 */
static bool update_vars(BTreeNode* ptr,
    int* last_child_idx_ptr,
    int child_idx,
    int* chain_end_child_idx_ptr,
    BTreeNode** last_over_min_cap_anc_ptr,
    ChildIdxCache* child_cache,
    MergeHintCache* merge_hint_cache,
    int* depth_ptr)
{
    // Deref
    BTreeNodeSib merge_cache_hint    = UNDEFINED;
    BTreeNode* last_over_min_cap_anc = *last_over_min_cap_anc_ptr;
    int depth                        = *depth_ptr;
    int last_child_idx               = *last_child_idx_ptr;
    int child_idx_after_merge        = child_idx;

    // Did the chain break?
    bool over_min_cap =
        btree_node_is_or_has_sib_over_min_cap(ptr, last_child_idx);

    // ALWAYS decrement this. The pred-leaf loses a key, and `ptr` will always
    // have the pred-leaf as a descendant.
    ptr->subtree_size -= 1;

    if (over_min_cap)
    {
        // Store the child-index of this node in case this node needs to borrow
        // a sibling.
        *chain_end_child_idx_ptr = last_child_idx;

        last_over_min_cap_anc    = ptr;
        depth                    = 0;

        merge_cache_hint         = compute_rotate_hint(ptr, last_child_idx);

        // If we're borrowing a key from our left sibling, the child index of
        // the next node is increased by one
        if (merge_cache_hint == LEFT)
        {
            child_idx_after_merge = child_idx + 1;
        }
    }
    else
    {
        merge_cache_hint = compute_merge_hint(ptr, last_child_idx);

        // If we're merging our left sibling, the next node's child index will
        // be incremented once for each of our left sibling's children. If we're
        // merging with our right sibling, all of it's children will appear
        // after the next node, so no change to its child index.
        if (merge_cache_hint == LEFT)
        {
            child_idx_after_merge =
                child_idx +
                btree_node_get_child(ptr->par, last_child_idx - 1)->curr_size;
        }
    }

    // Update merge hint cache
    if (!update_merge_hint_cache(merge_hint_cache, depth, merge_cache_hint))
    {
        return false;
    }

    // Update child idx cache
    if (!update_child_hint_cache(child_cache, depth, child_idx_after_merge))
    {
        return false;
    }

    *last_over_min_cap_anc_ptr = last_over_min_cap_anc;
    *depth_ptr                 = depth;
    *last_child_idx_ptr        = child_idx;

    return true;
}

/**
 * @brief Swaps the key with its predecessor and removes the predecessor
 * from its leaf
 *
 * @details Decreases the subtree sizes of all ancestors before and equal to
 * the last ancestor of the leaf either containing key or from which the
 * predecessor was removed for which the subtree size or the subtree size of
 * a sibling is over minimum capacity. The algorithm is as follows. We exclude
 * cache and subtree size updates for simplicity:
 *
 * @par Algorithm
 *
 * 0.  Set A <- `root`
 *
 * 1.  WHILE C is undefined, do:
 *
 * 2.      IF A contains `k`, do
 *
 * 3.          store A in C
 *
 * 4.      ELSE, do
 *
 * 5.          IF A is a leaf, do
 *
 * 6.              BREAK
 *
 * 7.          ELSE, do
 *
 * 5.              SET A <- next child
 *
 * 6.          END IF
 *
 * 7       END IF
 *
 * 7.  END WHILE
 *
 * 8.  IF C is not defined       // if we didn't find the key
 *
 * 9.      RETURN error
 *
 * 10. Store C in A
 *
 * // Find the closest thing to `key` in the subtree rooted at C
 *
 * 11. WHILE A is not a leaf, do
 *
 * 13.     SET A <- next child
 *
 * 14. END WHILE
 *
 * // Now, A should contain the predecessor
 *
 * 15. IF C = A, do
 *
 * 16.     Remove `key` from C
 *
 * 16. ELSE, do
 *
 * 16.     SET C[i] <- last key of A
 *
 * 17.     Pop the last key of A
 *
 * 18. END IF
 *
 *
 * TODO: Finish this paragraph
 * Define the "min-cap chain" to be the chain of... Define the "pred-leaf" to be
 * the ... In addition to (potentially) swapping the key with it's predecessor,
 * then deleting either the predecessor or the key from the pred-leaf, we're
 * also responsible for:
 *
 *     1. updating the subtree sizes of all ancestors preceding the last node
 *        in the min-cap chain
 *
 *     2. Storing, for each node in the min-cap chain and the node preceding the
 *        first element in the min-cap chain, the index of the child
 *        (potentially post-merge) whose subtree contains the pred-leaf
 *
 *     3. Storing, for each node in the min-cap chain, which sibling to merge
 *        with
 *
 *     4. Determining and storing which sibling (if any) the first node
 *        preceding the min-cap chain should borrow a key from
 *
 * @par Assumptions
 *
 *   - A descendent of `root` contains `key`
 *
 * @par Tech Notes
 *
 *   - Each memory-allocating function is responsible for cleaning up the
 * entire state's memory
 *
 *   - We don't output the cache sizes since the cache should always be
 * equal to the distance between last_over_min_cap_anc and the leaf where
 * `key` is.
 *
 *      - TODO: Add a redundancy preprocessor directive that causes the
 * cache sizes to be output and checked for sufficiently exhaustive
 *              redundancy settings
 *
 * @param root
 * @param key
 * @param last_over_min_cap_anc_ptr
 * @param child_idx_cache
 *
 * @return A return code
 *    - 0: Error
 *    - 1: OK
 */
int btree_node_find_closest_over_min_cap_anc(BTreeNode* root,
    int key,
    BTreeNode** last_over_min_cap_anc_ptr,
    int* chain_end_child_idx_ptr,  // TODO: Rename
    BTreeNodeSib** merge_hint_cache_ptr,
    int** child_idx_cache_ptr)
{
    // clang-format off
    // - it screws with my struct initialization style preference
    ChildIdxCache child_idx_cache = 
    {
      .data = (int*)malloc(sizeof(int) * DEFAULT_CHILD_IDX_CACHE_SIZE),
      .size = DEFAULT_CHILD_IDX_CACHE_SIZE
    };
    
    MergeHintCache merge_hint_cache = 
    {
      .data = (BTreeNodeSib*)malloc( sizeof(BTreeNodeSib) * DEFAULT_SIB_TO_MERGE_WITH_CACHE_SIZE),
      .size = DEFAULT_SIB_TO_MERGE_WITH_CACHE_SIZE
    };  // clang-format on

    // Search for a node containing `key`
    int child_idx                    = -1;
    int last_child_idx               = -1;
    int depth                        = 0;
    bool found_key                   = 0;
    bool encountered_leaf            = 0;
    int chain_end_child_idx          = 0;
    BTreeNode* a                     = root;
    BTreeNode* c                     = NULL;
    BTreeNode* last_over_min_cap_anc = NULL;

    while (c == NULL && !encountered_leaf)
    {
        child_idx = compute_child_idx(a, key, &found_key);

        if (found_key)
        {
            c = a;
        }
        else
        {
            if (btree_node_is_leaf(a))
            {
                encountered_leaf = true;
            }
            else
            {
                // Update subtree sizes, update child index cache, and either
                // merge hint cache or rotate hint
                if (!update_vars(a, &last_child_idx, child_idx,
                        &chain_end_child_idx, &last_over_min_cap_anc,
                        &child_idx_cache, &merge_hint_cache, &depth))
                    return 0;

                a = btree_node_get_child(a, child_idx);

                // Update depth
                depth += 1;
            }
        }
    }

    if (c == NULL)
    {
        // Error: key not in tree
        return 0;
    }

    while (!btree_node_is_leaf(a))
    {
        child_idx = compute_child_idx(a, key, &found_key);

        // Update subtree sizes, update child index cache, and either merge hint
        // cache or rotate hint
        if (!update_vars(a, &last_child_idx, child_idx, &last_over_min_cap_anc,
                &chain_end_child_idx, &child_idx_cache, &merge_hint_cache,
                &depth))
            return 0;

        // If A contains `key`, it will be set to the child BEFORE
        // `key`
        a = btree_node_get_child(a, child_idx);

        depth += 1;
    }

    if (c != a)
    {
        int pred = btree_node_get_last_key(a);

#if BTREE_DEBUG_1 != 1
        btree_node_set_key(c, child_idx, pred);
#endif

        child_idx = a->curr_size - 1;
    }

    // Update subtree sizes, update child index cache, and either merge hint
    // cache or rotate hint
    if (!update_vars(a, &last_child_idx, child_idx, &chain_end_child_idx,
            &last_over_min_cap_anc, &child_idx_cache, &merge_hint_cache,
            &depth))
        return 0;

    // Remove `pred` or `key` from pred_leaf
#if BTREE_DEBUG_1 != 1
    btree_node_remove_key(a, child_idx);
#endif

    *last_over_min_cap_anc_ptr = last_over_min_cap_anc;
    *merge_hint_cache_ptr      = merge_hint_cache.data;
    *child_idx_cache_ptr       = child_idx_cache.data;
    *chain_end_child_idx_ptr   = chain_end_child_idx;

    return 1;
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
    BTreeNode* lsib, BTreeNode* rsib, int pivot_idx)
{
    // append pivot and `rsib`'s first child to the back of lsib
    // replace pivot with first key of `rsib`
    int rsib_first_key = 0;

    btree_node_pop_front_key(rsib, &rsib_first_key);
    rsib->subtree_size -= 1;

    btree_node_push_back_key(lsib, btree_node_get_key(lsib->par, pivot_idx));
    lsib->subtree_size += 1;

    if (!btree_node_is_leaf(lsib))
    {
        BTreeNode* rsib_first_child = NULL;

        btree_node_pop_front_child(rsib, &rsib_first_child);
        rsib->subtree_size -= rsib_first_child->subtree_size;

        // TODO: rsib_first_child may be null. Check for this

        btree_node_set_last_child(lsib, rsib_first_child);
        lsib->subtree_size += rsib_first_child->subtree_size;
    }

    btree_node_set_key(lsib->par, pivot_idx, rsib_first_key);
}

/**
 * @brief Rotate right about `pivot_idx`
 *
 * @details
 *
 * @par Assumptions
 */
static void btree_node_rotate_right(
    BTreeNode* lsib, BTreeNode* rsib, int pivot_idx)
{
    // append pivot and `lsib`'s last child to the front of rsib
    // replace pivot with last key of `lsib`
    int lsib_last_val = 0;

    btree_node_pop_back_key(lsib, &lsib_last_val);
    lsib->subtree_size -= 1;

    btree_node_push_front_key(rsib, btree_node_get_key(rsib->par, pivot_idx));
    rsib->subtree_size += 1;

    if (!btree_node_is_leaf(lsib))
    {
        BTreeNode* lsib_last_child = NULL;

        btree_node_pop_back_child(lsib, &lsib_last_child);
        lsib->subtree_size -= lsib_last_child->subtree_size;

        // TODO: lsib_last_child may be null. Check for this

        btree_node_push_front_child(rsib, lsib_last_child);
        rsib->subtree_size += lsib_last_child->subtree_size;
    }

    btree_node_set_key(rsib->par, pivot_idx, lsib_last_val);
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
        btree_node_copy_child_range(
            lsib, rsib, lsib->curr_size + 1, 0, rsib->curr_size + 1);
    }

    btree_node_push_back_key(lsib, btree_node_get_key(par, sep_idx));
    lsib->subtree_size += 1;

    btree_node_append_key_range(lsib, rsib, 0, rsib->curr_size);
    lsib->subtree_size += rsib->subtree_size;

    btree_node_kill(rsib);

    // Parent loses the pivot key, but its subtree size doens't change
    btree_node_remove_key(par, sep_idx);
    btree_node_remove_child(par, sep_idx + 1);
}

int btree_node_delete_impl(BTreeNode* root, int val, BTreeNode** new_root_ptr)
{
    if (!btree_node_contains_key(root, val))
    {
        return 2;
    }

    BTreeNode* ptr                 = NULL;
    int* child_idx_cache           = NULL;
    BTreeNodeSib* merge_hint_cache = NULL;
    int chain_end_child_idx        = -1;

    if (!btree_node_find_closest_over_min_cap_anc(root, val, &ptr,
            &chain_end_child_idx, &merge_hint_cache, &child_idx_cache))
    {
        // TODO: Handle appropriately. This could be an OOM error or a
        // detected race condition.
        return 0;
    }

    // The last routine deletes the key from the tree, and leaves a leaf
    // under min capacity. Now we need to rebalance the tree.

    int depth        = 0;
    BTreeNode* left  = NULL;
    BTreeNode* right = NULL;

    if (ptr == NULL)  // TODO: Redundant check for root of the correct form
    {
        // Root is squashed
        left  = btree_node_get_child(root, 0);
        right = btree_node_get_child(root, 1);

        btree_node_merge_sibs(left, right, root, 0);
        *new_root_ptr = left;
    }
    else
    {
        int rotate_hint = merge_hint_cache[depth];

        if (rotate_hint != NEITHER)
        {
            btree_node_get_sibs(ptr, child_idx_cache[depth], &left, &right);

            if (rotate_hint == LEFT)
            {
                // left sibling can spare a key
                int pivot_idx = chain_end_child_idx - 1;
                btree_node_rotate_right(left, ptr, pivot_idx);
            }
            else if (rotate_hint == RIGHT)
            {
                // right sibling can spare a key.
                int pivot_idx = chain_end_child_idx;
                btree_node_rotate_left(ptr, right, pivot_idx);
            }
            else if (rotate_hint == UNDEFINED)
            {
                // TODO: Manually determine which way to rotate
                return 0;
            }
        }
    }

    left  = ptr;
    right = NULL;

    while (!btree_node_is_leaf(left))
    {
        // Descend
        left = btree_node_get_child(left, child_idx_cache[depth]);
        depth += 1;

        // Merge
        BTreeNode* par = left->par;
        int sep_idx    = 0;
        if (merge_hint_cache[depth] == LEFT)
        {
            right   = left;
            sep_idx = child_idx_cache[depth - 1] - 1;
            left    = btree_node_get_child(par, sep_idx);
        }
        else if (merge_hint_cache[depth] == RIGHT)
        {
            sep_idx = child_idx_cache[depth - 1];
            right   = btree_node_get_child(par, sep_idx + 1);
        }

        btree_node_merge_sibs(left, right, par, sep_idx);
    }

    return 1;
}