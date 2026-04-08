#include "./btree.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include "./btree_key.h"
#include "./btree_node.h"
#include "./mem.h"

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
static void btree_subtree_kill(BTreeNode* node)
{
    if (node == NULL) return;

    if (node->children)
    {
        for (size_t i = 0; i <= btree_node_curr_size(node); i++)
            btree_subtree_kill(btree_node_get_child(node, i));
    }

    btree_node_kill(node);
}

void btree_kill(BTree* tree)
{
    btree_subtree_kill(tree->root);
    free(tree);
}

// TODO: This is causing the compiler to freak out. You need to figure out how
// this works depending on whether the node size is predefined or not.
BTree* btree_init(size_t order)
{
    BTree* tree = (BTree*)jl_btree_malloc(sizeof(BTree));
    if (tree == NULL)
    {
        return NULL;
    }

    // TODO: TERRIBLE solution. Do this differently.
#ifdef BTREE_NODE_NODE_SIZE
    order = BTREE_NODE_NODE_SIZE;
#endif

    BTreeNode* root = NULL;
    if (!btree_node_init(order, &root, false))
    {
        free(tree);
        return NULL;
    }

    tree->root = root;

    return tree;
}

////////////////////////////////////////////////////////////////////////////////
// INSERTION                                                                  //
////////////////////////////////////////////////////////////////////////////////

int btree_insert(BTree* tree, BTreeKey2* key)
{
    BTreeNode* new_root = NULL;
    int res    = btree_node_insert_impl(tree->root, key, &new_root, tree->cmp);
    tree->root = new_root;
    return res;
}

////////////////////////////////////////////////////////////////////////////////
// DELETION                                                                   //
////////////////////////////////////////////////////////////////////////////////

// Implemented in "delete.h"
// TODO: This is where the API will be for delete

int btree_delete(BTree* tree, BTreeKey2* key)
{
    BTreeNode* new_root = NULL;
    int res    = btree_node_delete_impl(tree->root, key, &new_root, tree->cmp);
    tree->root = new_root;
    return res;
}