#include "./btree.h"

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "./btree_node.h"
#include "./btree_settings.h"
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
        for (int i = 0; i <= btree_node_curr_size(node); i++)
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

////////////////////////////////////////////////////////////////////////////////
// DELETION                                                                   //
////////////////////////////////////////////////////////////////////////////////

// Implemented in "delete.h"
// TODO: This is where the API will be for delete