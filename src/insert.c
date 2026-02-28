#include "./insert.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "./btree.h"
#include "./btree_node.h"
#include "./btree_settings.h"
#include "./search.h"
#include "./stack.h"

// Child index will depend on which half of `ptr` the next ancestor belongs to
static size_t child_idx_after_split(
    const BTreeNode* ptr, const BTreeKey key, const size_t child_idx)
{
    const size_t mid = btree_node_node_size(ptr) / 2;
    if (btree_node_is_full(ptr) && key > btree_node_get_key(ptr, mid))
        return child_idx - mid - 1;

    return child_idx;
}

static void update_subtree_sizes_upwards(
    BTreeNode* node, const BTreeNode* stop_at, const int inc)
{
    while (node != NULL && node != stop_at)
    {
        btree_node_inc_subtree_size(node, inc);
        node = btree_node_par(node);
    }
}

static size_t compute_child_idx(BTreeNode* node, BTreeKey key, bool* found_key)
{
    size_t child_idx = 0;
    *found_key       = 0;

    if (btree_node_get_last_key(node) < key)
    {
        child_idx = btree_node_num_keys(node);
    }
#if REDUNDANT < 1
    else if (btree_node_get_last_key(node) == key)
    {
        *found_key = 1;
    }
#endif
    else if (btree_node_get_first_key(node) > key)
    {
        child_idx = 0;
    }
    else
    {
        child_idx = binary_search(
            btree_node_keys(node), 0, btree_node_num_keys(node), key);
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
#endif
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
bool btree_node_find_closest_nonfull_anc(const BTreeNode* root,
    const BTreeKey key,
    BTreeNode** last_nonfull_anc_ptr,
    Stack* child_hint_cache_stack)
{
    // Depth in the subtree rooted at last_nonfull_anc. Assume a depth of 1
    // in case all ancestors are full. Otherwise, depth will be reset to
    // zero.
    // Always starts at zero because a new root is never split
    size_t child_idx = 0;
    if (!stack_push(child_hint_cache_stack, &child_idx))
    {
        return 0;
    }

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
            stack_clear(child_hint_cache_stack);
        }

        // Find the next ancestor
        // TODO: Only compute the ``after split" child index if we know this
        // node will be split
        child_idx = compute_child_idx(ptr, key, &found_key);
        size_t child_idx_after_split_var =
            child_idx_after_split(ptr, key, child_idx);

        if (found_key)
        {
            return 0;
        }

        if (!stack_push(child_hint_cache_stack, &child_idx_after_split_var))
        {
            // OOM
            return 0;
        }

        btree_node_intl_descend(&ptr, child_idx);
    }

    if (!btree_node_is_full(ptr))
    {
        update_subtree_sizes_upwards(ptr, last_nonfull_anc, +1);
        last_nonfull_anc = ptr;
        stack_clear(child_hint_cache_stack);
    }

    // Find the next ancestor
    // TODO: Only compute the ``after split" child index if we know this
    // node will be split
    child_idx = compute_child_idx(ptr, key, &found_key);
    size_t child_idx_after_split_var =
        child_idx_after_split(ptr, key, child_idx);

    if (found_key)
    {
        return 0;
    }

    if (!stack_push(child_hint_cache_stack, &child_idx_after_split_var))
    {
        // OOM
        return 0;
    }

#if REDUNDANT < 1
    // TODO: find_idx... is broken
    if (key == btree_node_get_key(ptr, binary_search(btree_node_keys(ptr), 0,
                                           btree_node_num_keys(ptr), key)))
    {
        return 0;
    }
#endif

    *last_nonfull_anc_ptr = last_nonfull_anc;

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
bool btree_node_split(
    BTreeNode* node, const BTreeNode** rsib_ptr, int* next_key_ptr)
{
    const size_t size = btree_node_node_size(node);
    // size of left sibling after the split
    const size_t new_left_size = size / 2;

    // Construct rsib
    BTreeNode* rsib;
    if (!btree_node_init(size, &rsib, !btree_node_is_leaf(node)))
    {
        // OOM
        return 0;
    }
    *rsib_ptr = rsib;

    //     1. Siblings
    btree_node_set_left_sib(rsib, node);
    btree_node_set_right_sib(rsib, btree_node_right_sib(node));

    //     2. Keys

    // TODO: Optimize
    btree_node_remove_key(node, new_left_size, next_key_ptr);
    btree_node_dec_subtree_size_1(node);
    for (size_t i = 0; i < size - new_left_size - 1; i++)
    {
        BTreeKey temp;
        btree_node_pop_back_key(node, &temp);
        btree_node_dec_subtree_size_1(node);
        btree_node_push_back_key(rsib, temp);
        btree_node_inc_subtree_size_1(rsib);
    }

    //     3. Children
    if (!btree_node_is_leaf(node))
    {
        for (size_t i = 0; i < size - new_left_size; i++)
        {
            BTreeNode* temp;
            btree_node_pop_back_child(node, &temp);
            btree_node_push_front_child(rsib, temp);
        }

        for (size_t i = 0; i < btree_node_num_children(rsib); i++)
        {
            BTreeNode* child = btree_node_get_child(rsib, i);
            btree_node_inc_subtree_size(rsib, btree_node_subtree_size(child));
            btree_node_dec_subtree_size(node, btree_node_subtree_size(child));
        }
    }

    // Amend node

    //     1. Siblings
    btree_node_set_right_sib(node, rsib);
    btree_node_set_left_sib(rsib, node);

    //     2. Keys
#ifdef BTREE_KEEP_UNUSED_MEM_CLEAN
    btree_node_clear_key_range(node, new_left_size, size - new_left_size);
#endif

    //     3. Children
#ifdef BTREE_KEEP_UNUSED_MEM_CLEAN
    if (!btree_node_is_leaf(node))
    {
        btree_node_clear_child_range(
            node, new_left_size + 1, size - new_left_size + 1);
    }
#endif

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
    BTreeNode* root, const BTreeKey key, const BTreeNode** new_root_ptr)
{
    // By default, the root of the tree doesn't change
    *new_root_ptr = root;

    if (btree_node_contains_key(root, key))
    {
        // Exit early if we find `key` in the tree.
        return 2;
    }

    Stack* child_idx_cache_stack =
        stack_init(sizeof(size_t), DEFAULT_CHILD_IDX_CACHE_SIZE);
    if (child_idx_cache_stack == NULL)
    {
        // TODO: Simply don't cache the children if this fails. Don't error out.
        return 0;
    }

    BTreeNode* a = NULL;

    if (!btree_node_find_closest_nonfull_anc(
            root, key, &a, child_idx_cache_stack))
    {
        stack_kill(child_idx_cache_stack);

        return 0;
    }

    size_t* child_hint_cache = child_idx_cache_stack->data;

    // Create a new root if all ancestors of the leaf we're inserting `key` into
    // are full
    if (a == NULL)
    {
        if (!btree_node_init(btree_node_node_size(root), &a, 1))
        {
            stack_kill(child_idx_cache_stack);

            return 0;
        }

        btree_node_push_front_child(a, root);
        // +1 because btree_node_find_closest_nonfull_anc didn't increment any
        // subtree sizes as all ancestors were full
        btree_node_inc_subtree_size(a, btree_node_subtree_size(root) + 1);
        *new_root_ptr = a;
    }

    // Insertion proceeds top-down from the first non-full ancestor of the leaf
    // where `key` will be inserted.
    size_t depth     = 0;
    size_t child_idx = 0;
    while (!btree_node_is_leaf(a))
    {
        // Get child B of A whose range contains `key`, and split B into B1, B2,
        // k (B1: lchild, B2: rchild, k: sep_key)
        BTreeNode* b1 = btree_node_get_child(a, child_hint_cache[depth]);
        BTreeNode* b2 = NULL;
        BTreeKey k    = 0;

        if (!btree_node_split(b1, &b2, &k))
        {
            stack_kill(child_idx_cache_stack);

            return 0;
        }

        // Insert (k, B2) into A
        //
        // We're double-incrementing theh subtree size of `a` here.
        // This is because We remove a key from `b1` during the split, which
        // between then and now decreases `a`'s subtree size without logging it.
        //
        // Basically, we're moving a key from `b1` to `a` and it gets counted as
        // adding a key `a`'s subtree.
        //
        // The same applies to `b2`. This is getting moved from `b1` to a new
        // node, but its subtree size is added to `a`'s here as if it's a child
        // with new keys.
        btree_node_insert_key(a, child_hint_cache[depth], k);
        btree_node_insert_child(a, child_hint_cache[depth] + 1, b2);

        // Descend
        a = key < k ? b1 : b2;

        btree_node_inc_subtree_size_1(a);

        depth += 1;
    }

    assert(child_hint_cache[depth] ==
           find_idx_of_min_key_greater_than_val(a, key));

    btree_node_insert_key(a, child_hint_cache[depth], key);

    return 1;
}