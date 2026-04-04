
#include "contains.h"

#include "search.h"

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
        if (btree_node_get_key(ptr, btree_node_curr_size(ptr) - 1) < key)
        {
            child_idx = btree_node_curr_size(ptr);
        }
        else if (btree_node_get_key(ptr, btree_node_curr_size(ptr) - 1) == key)
        {
            return 1;
        }
        else if (btree_node_get_key(ptr, 0) > key)
        {
            child_idx = 0;
        }
        else
        {
            child_idx =
                binary_search(ptr->keys, 0, btree_node_curr_size(ptr), key);
            if (btree_node_get_key(ptr, child_idx) == key) return 1;
            child_idx += 1;
        }

        btree_node_intl_descend(&ptr, child_idx);
    }

    // Check if the leaf contains `key`
    if (btree_node_get_key(ptr,
            binary_search(ptr->keys, 0, btree_node_curr_size(ptr), key)) == key)
    {
        return 1;
    }

    return 0;
}