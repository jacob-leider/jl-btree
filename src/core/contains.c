
#include "./contains.h"

#include "./search.h"

/** @brief Determine whether a descendent of `root` contains the key `key`
 *
 *  @param[in] root the root of a btree
 *  @param[in] key the key we're searching for
 *
 *  @return a return code
 *     - 0: tree doesn't contain `key`
 *     - 1: tree contains `key`
 */
int btree_node_contains_key(
    BTreeNode* root, BTreeKey2* key, BTreeKeyComparator cmp)
{
    BTreeNode* ptr = root;
    int child_idx  = 0;

    // Search for a node containing `key`
    while (!btree_node_is_leaf(ptr))
    {
        // TODO: Get rid of redundant key comparisons
        if (btree_key_lt(btree_node_get_key(ptr, btree_node_curr_size(ptr) - 1),
                key, cmp))
        {
            child_idx = btree_node_curr_size(ptr);
        }
        else if (btree_key_eq(
                     btree_node_get_key(ptr, btree_node_curr_size(ptr) - 1),
                     key, cmp))
        {
            return 1;
        }
        else if (btree_key_gt(btree_node_get_key(ptr, 0), key, cmp))
        {
            child_idx = 0;
        }
        else
        {
            child_idx = binary_search_2(ptr->keys, 0, btree_node_curr_size(ptr),
                key, sizeof(BTreeKey2*), cmp);

            if (btree_key_eq(btree_node_get_key(ptr, child_idx), key, cmp))
            {
                return 1;
            }

            child_idx += 1;
        }

        btree_node_intl_descend(&ptr, child_idx);
    }

    // Check if the leaf contains `key`
    // TODO: Clean up/refactor
    if (btree_key_eq(btree_node_get_key(ptr, binary_search_2(ptr->keys, 0,
                                                 btree_node_curr_size(ptr), key,
                                                 sizeof(BTreeKey2*), cmp)),
            key, cmp))
    {
        return 1;
    }

    return 0;
}