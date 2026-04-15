
#include "contains.h"

#include "./btree_key.h"
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
int btree_node_contains_key(BTreeNode* root, BTreeKey* key)
{
    BTreeNode* ptr   = root;

    size_t child_idx = 0;
    bool found       = false;
    bool ptr_is_leaf = false;

    // Search for a node containing `key`
    while (!found && !ptr_is_leaf)
    {
        child_idx = find_idx_of_min_key_greater_than_val(ptr, key, &found);

        if (btree_node_is_leaf(ptr))
        {
            ptr_is_leaf = true;
        }
        else
        {
            btree_node_intl_descend(&ptr, child_idx);
        }
    }

    // Check if the leaf contains `key`

    return found;
}