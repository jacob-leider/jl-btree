#include "delete.h"

#include <assert.h>
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
#include "stack.h"

////////////////////////////////// Purpose /////////////////////////////////////
///
/// Implement B-Tree key-deletion
///
//////////////////////////////// Terminology ///////////////////////////////////
///
/// Consider a btree rooted at `root` containing key `key`. The following
/// definitions are rigorous, and used in documentation throughout this file.
///
///     * "can spare"
///
/// A node of order `t` "can spare" if it has at least `2*t - 1` keys
////
///     * "can borrow"
///
/// Short for "can borrow a key from a sibling." A node of order `t` "can
/// borrow" if it has a sibling with at least `2*t - 1` keys
///
///     * "pred-leaf"
///
/// The leaf containing the predecessor to `key`
///
///     * "min-cap chain"
///
/// The longest contiguous sequence of nodes that cannot spare or borrow a key
/// (due to siblings being at minimum capcity) that contains the pred-leaf. If
/// the pred-leaf can borrow or spare a key, then the min-cap chain is said to
/// be empty.
///
///     * "closest over-min-cap ancestor
///
/// The deepest ancestor of `root` that can spare or can borrow.
///
////////////////////////////////////////////////////////////////////////////////

// Determines which of `node`'s children is the root of a subtree containg `key`
static size_t compute_child_idx(BTreeNode* node, BTreeKey key, bool* found_key)
{
    size_t child_idx = 0;
    *found_key       = 0;

    if (btree_node_curr_size(node) == 0)
    {
        // Error. Subtracting 1 causes underflow.
        printf("Underflow risk in %s at line %d\n", __FILE__, __LINE__);
    }

    if (btree_node_get_key(node, btree_node_curr_size(node) - 1) < key)
    {
        child_idx = btree_node_curr_size(node);
    }
#if REDUNDANT < 1
    else if (btree_node_get_key(node, btree_node_curr_size(node) - 1) == key)
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
        child_idx =
            binary_search(node->keys, 0, btree_node_curr_size(node), key);
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
static bool btree_node_can_spare_or_borrow_key(
    BTreeNode* node, size_t child_idx)
{
    if (btree_node_over_min_cap(node))
    {
        return true;
    }

    // TODO: Fix this terminology in function names
    // Root and min capacity => not "over min capacity"
    if (btree_node_is_root(node))
    {
        // Minimum capacity for a root is 1
        return btree_node_curr_size(node) > 1;
    }

    BTreeNode* left  = NULL;
    BTreeNode* right = NULL;
    btree_node_get_sibs(node, child_idx, &left, &right);

    if (btree_node_left_sib(node) != left)
    {
        printf("Left sib not stored on node\n");

        printf("\t-Node: ");
        printArr(btree_node_keys(node), btree_node_curr_size(node));

        printf("\t-Stored: ");
        if (btree_node_left_sib(node) != NULL)
        {
            printArr(btree_node_keys(btree_node_left_sib(node)),
                btree_node_curr_size(btree_node_left_sib(node)));
        }
        else
        {
            printf("NULL\n");
        }

        printf("\t-Actual: ");
        if (left != NULL)
        {
            printArr(btree_node_keys(left), btree_node_curr_size(left));
        }
        else
        {
            printf("NULL\n");
        }
    }

    if (btree_node_left_sib(node) != NULL &&
        btree_node_over_min_cap(btree_node_left_sib(node)))
    {
        return true;
    }

    if (btree_node_right_sib(node) != NULL &&
        btree_node_over_min_cap(btree_node_right_sib(node)))
    {
        return true;
    }

    return false;
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
BTreeNodeSib compute_merge_hint(BTreeNode* node, size_t child_idx)
{
    if (btree_node_over_min_cap(node))
    {
        return NEITHER;
    }

    if (btree_node_is_root(node))
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

BTreeNodeSib compute_rotate_hint(BTreeNode* node, size_t child_idx)
{
    if (btree_node_over_min_cap(node))
    {
        return NEITHER;
    }

    if (btree_node_is_root(node))
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

typedef struct BTreeNodeDeleteState
{
    Stack* child_hint_cache_stack;
    Stack* merge_hint_cache_stack;
    BTreeNode* last_over_min_cap_anc;
    size_t child_idx;
    size_t last_child_idx;
} BTreeNodeDeleteState;

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
static bool update_vars(BTreeNode* ptr, BTreeNodeDeleteState* state)
{
    // Deref
    BTreeNodeSib merge_hint          = UNDEFINED;
    Stack* merge_hint_cache_stack    = state->merge_hint_cache_stack;
    Stack* child_hint_cache_stack    = state->child_hint_cache_stack;
    BTreeNode* last_over_min_cap_anc = state->last_over_min_cap_anc;
    size_t last_child_idx            = state->last_child_idx;
    size_t child_idx                 = state->child_idx;
    size_t child_idx_after_merge     = state->child_idx;

    // ALWAYS decrement this. The pred-leaf loses a key, and `ptr` will always
    // have the pred-leaf as a descendant.
    btree_node_dec_subtree_size_1(ptr);

    if (btree_node_can_spare_or_borrow_key(ptr, last_child_idx))
    {
        last_over_min_cap_anc = ptr;

        stack_clear(merge_hint_cache_stack);

        // Store the child-index of this node in case this node needs to borrow
        // a sibling.
        size_t stack_top = 0;
        stack_get_top(child_hint_cache_stack, &stack_top);
        stack_clear(child_hint_cache_stack);
        stack_push(child_hint_cache_stack, &stack_top);

        merge_hint = compute_rotate_hint(ptr, last_child_idx);

        // If we're borrowing a key from our left sibling, the child index of
        // the next node is increased by one
        if (merge_hint == LEFT)
        {
            child_idx_after_merge = child_idx + 1;
        }
    }
    else
    {
        merge_hint = compute_merge_hint(ptr, last_child_idx);

        // If we're merging our left sibling, the next node's child index will
        // be incremented once for each of our left sibling's children. If we're
        // merging with our right sibling, all of it's children will appear
        // after the next node, so no change to its child index.
        if (merge_hint == LEFT)
        {
            if (last_child_idx == 0)
            {
                printf("Underflow risk in %s at line %d\n", __FILE__, __LINE__);
                return false;
            }

            BTreeNode* left_sib =
                btree_node_get_child(btree_node_par(ptr), last_child_idx - 1);

            child_idx_after_merge = child_idx + btree_node_curr_size(left_sib);
        }
    }

    // Update merge hint cache
    if (!stack_push(merge_hint_cache_stack, &merge_hint))
    {
        return false;
    }

    // Update child idx cache
    if (!stack_push(child_hint_cache_stack, &child_idx_after_merge))
    {
        return false;
    }

    state->last_over_min_cap_anc = last_over_min_cap_anc;
    state->last_child_idx        = child_idx;

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
int btree_node_delete_key(BTreeNode* root,
    BTreeKey key,
    BTreeNode** last_over_min_cap_anc_ptr,
    Stack* child_hint_cache_stack,
    Stack* merge_hint_cache_stack)
{
    // Search for a node containing `key`
    // clang-format off
    BTreeNodeDeleteState state = {
        .last_over_min_cap_anc  = NULL,
        .child_hint_cache_stack = child_hint_cache_stack,        
        .merge_hint_cache_stack = merge_hint_cache_stack,
        .child_idx              = 0,
        .last_child_idx         = 0 
    };
    // clang-format on

    bool found_key        = 0;
    bool encountered_leaf = 0;
    BTreeNode* a          = root;
    BTreeNode* c          = NULL;

    while (c == NULL && !encountered_leaf)
    {
        state.child_idx = compute_child_idx(a, key, &found_key);

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
                if (!update_vars(a, &state))
                {
                    return 0;
                }

                a = btree_node_get_child(a, state.child_idx);
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
        state.child_idx = compute_child_idx(a, key, &found_key);

        // Update subtree sizes, update child index cache, and either merge hint
        // cache or rotate hint
        if (!update_vars(a, &state))
        {
            return 0;
        }

        // If A contains `key`, it will be set to the child BEFORE
        // `key`
        a = btree_node_get_child(a, state.child_idx);
    }

    if (c != a)
    {
        BTreeKey pred = btree_node_get_last_key(a);

        btree_node_set_key(c, state.child_idx, pred);

        // Underflow risk. TODO: Should this really be an assert?
        assert(btree_node_curr_size(a) > 0);

        state.child_idx = btree_node_curr_size(a) - 1;
    }

    // Update subtree sizes, update child index cache, and either merge hint
    // cache or rotate hint
    if (!update_vars(a, &state)) return 0;

    // Remove `pred` or `key` from pred_leaf
#if BTREE_DEBUG_1 != 1
    btree_node_remove_key(a, state.child_idx, NULL);
#endif

    *last_over_min_cap_anc_ptr = state.last_over_min_cap_anc;

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
    BTreeNode* lsib, BTreeNode* rsib, size_t pivot_idx)
{
    // append pivot and `rsib`'s first child to the back of lsib
    // replace pivot with first key of `rsib`
    size_t rsib_first_key       = 0;
    BTreeNode* rsib_first_child = NULL;

    btree_node_pop_front_key(rsib, &rsib_first_key);

    btree_node_push_back_key(
        lsib, btree_node_get_key(btree_node_par(lsib), pivot_idx));

    if (!btree_node_is_leaf(lsib))
    {
        btree_node_pop_front_child(rsib, &rsib_first_child);

        // TODO: Should this really be an assert?
        assert(rsib_first_child != NULL);

        btree_node_push_back_child(lsib, rsib_first_child);
    }

    // Deal with subtree sizes
    btree_node_dec_subtree_size_1(rsib);

    btree_node_inc_subtree_size_1(lsib);

    if (!btree_node_is_leaf(lsib))
    {
        assert(rsib_first_child != NULL);

        btree_node_dec_subtree_size(
            rsib, btree_node_subtree_size(rsib_first_child));

        btree_node_inc_subtree_size(
            lsib, btree_node_subtree_size(rsib_first_child));
    }

    btree_node_set_key(btree_node_par(lsib), pivot_idx, rsib_first_key);
}

/**
 * @brief Rotate right about `pivot_idx`
 *
 * @details
 *
 * @par Assumptions
 */
static void btree_node_rotate_right(
    BTreeNode* lsib, BTreeNode* rsib, size_t pivot_idx)
{
    // append pivot and `lsib`'s last child to the front of rsib
    // replace pivot with last key of `lsib`
    BTreeKey lsib_last_val     = 0;
    BTreeNode* lsib_last_child = NULL;

    btree_node_pop_back_key(lsib, &lsib_last_val);

    btree_node_push_front_key(
        rsib, btree_node_get_key(btree_node_par(rsib), pivot_idx));

    if (!btree_node_is_leaf(lsib))
    {
        btree_node_pop_back_child(lsib, &lsib_last_child);

        // TODO: Should this really be an assert?
        assert(lsib_last_child != NULL);

        btree_node_push_front_child(rsib, lsib_last_child);
    }

    // Deal with subtree sizes
    btree_node_dec_subtree_size_1(lsib);

    btree_node_inc_subtree_size_1(rsib);

    if (!btree_node_is_leaf(lsib))
    {
        assert(lsib_last_child != NULL);

        btree_node_dec_subtree_size(
            lsib, btree_node_subtree_size(lsib_last_child));

        btree_node_inc_subtree_size(
            rsib, btree_node_subtree_size(lsib_last_child));
    }

    btree_node_set_key(btree_node_par(rsib), pivot_idx, lsib_last_val);
}

/**
 * @brief Merge `lsib` and `rsib`, storing the result in `lsib`
 *
 * @details the pivot key, then `right`'s keys and (if `left` is internal)
 * `right`'s children are appended to `left`. `right` is destroyed.
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
    BTreeNode* left, BTreeNode* right, BTreeNode* par, size_t sep_idx)
{
    // 1. Amend left sibling
    //     a. Keys
    btree_node_push_back_key(left, btree_node_get_key(par, sep_idx));
    btree_node_append_key_range(left, right, 0, btree_node_num_keys(right));

    //     b. Children
    if (!btree_node_is_leaf(left))
    {
        btree_node_append_child_range(
            left, right, 0, btree_node_num_children(right));
    }

    btree_node_inc_subtree_size(left, btree_node_subtree_size(right) + 1);

    // 2. Amend par
    //     a. Keys
    btree_node_remove_key(par, sep_idx, NULL);
    //     b. Children
    btree_node_remove_child(par, sep_idx + 1, NULL);

    // 3. Amend right sibling
    btree_node_kill(right);
}

int btree_node_delete_impl(
    BTreeNode* root, BTreeKey val, BTreeNode** new_root_ptr)
{
#if REDUNDANT > 1
    if (root == NULL)
    {
        return 0;
    }
#endif

    if (!btree_node_contains_key(root, val))
    {
        return 2;
    }

    Stack* child_hint_cache_stack =
        stack_init(sizeof(size_t), DEFAULT_CHILD_IDX_CACHE_SIZE);
    if (child_hint_cache_stack == NULL)
    {
        // TODO: Simply don't cache the children if this fails. Don't error out.
        return false;
    }

    Stack* merge_hint_cache_stack =
        stack_init(sizeof(BTreeNodeSib), DEFAULT_CHILD_IDX_CACHE_SIZE);
    if (merge_hint_cache_stack == NULL)
    {
        // TODO: Simply don't cache the children if this fails. Don't error out.
        return false;
    }

    BTreeNode* ptr = NULL;

    if (!btree_node_delete_key(
            root, val, &ptr, child_hint_cache_stack, merge_hint_cache_stack))
    {
        // TODO: Handle appropriately. This could be an OOM error or a
        // detected race condition.
        printf("Delete key returned 0.\n");
        return 0;
    }

    BTreeNodeSib* merge_hint_cache = merge_hint_cache_stack->data;
    size_t* child_hint_cache       = child_hint_cache_stack->data;

    size_t chain_end_child_idx     = child_hint_cache[0];

    // The last routine deletes the key from the tree, and leaves a leaf
    // under min capacity. Now we need to rebalance the tree.

    size_t depth     = 0;
    BTreeNode* left  = NULL;
    BTreeNode* right = NULL;

    if (ptr == NULL)  // TODO: Redundant check for root of the correct form
    {
        if (btree_node_curr_size(root) > 1)
        {
            // Error: root should only be squashed if it has exactly two
            // children and neither can spare a key
            stack_kill(merge_hint_cache_stack);
            stack_kill(child_hint_cache_stack);

            return 0;
        }

        // Root is squashed
        left  = btree_node_get_child(root, 0);
        right = btree_node_get_child(root, 1);

        btree_node_merge_sibs(left, right, root, 0);

        *new_root_ptr = left;
    }
    else
    {
        BTreeNodeSib rotate_hint = merge_hint_cache[0];

        // TODO: Validate that siblings are stored on ptr

        if (rotate_hint != NEITHER)
        {
            if (rotate_hint == LEFT)
            {
                // left sibling can spare a key
                // TODO: Check that chain_end_child_idx is > 0 and handle in the
                // redundant case
                btree_node_rotate_right(
                    btree_node_left_sib(ptr), ptr, chain_end_child_idx - 1);
            }
            else if (rotate_hint == RIGHT)
            {
                // right sibling can spare a key.
                btree_node_rotate_left(
                    ptr, btree_node_right_sib(ptr), chain_end_child_idx);
            }
            else if (rotate_hint == UNDEFINED)
            {
                // TODO: Manually determine which way to rotate
                stack_kill(merge_hint_cache_stack);
                stack_kill(child_hint_cache_stack);

                return 0;
            }
        }
    }

    left  = ptr;
    right = NULL;

    while (!btree_node_is_leaf(left))
    {
        size_t child_idx = child_hint_cache[depth + 1];
        left             = btree_node_get_child(left, child_idx);
        depth += 1;

        // Merge
        BTreeNode* par          = btree_node_par(left);
        size_t sep_idx          = 0;

        BTreeNodeSib merge_hint = merge_hint_cache[depth];
        if (merge_hint == LEFT)
        {
            sep_idx = child_idx - 1;
            right   = left;
            left    = btree_node_get_child(par, sep_idx);
        }
        else if (merge_hint == RIGHT)
        {
            sep_idx = child_idx;
            right   = btree_node_get_child(par, sep_idx + 1);
        }
        // TODO: Else, error

        btree_node_merge_sibs(left, right, par, sep_idx);
    }

    // Free rebalance hint caches
    stack_kill(merge_hint_cache_stack);
    stack_kill(child_hint_cache_stack);

    return 1;
}