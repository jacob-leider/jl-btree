#ifndef __BTREE_DELETE_H__
#define __BTREE_DELETE_H__

#include <stdbool.h>

#include "./btree_node.h"

typedef struct BTreeNode BTreeNode;

typedef enum BTreeNodeSib
{
    UNDEFINED,  // Typically indicates an error
    NEITHER,
    LEFT,
    RIGHT
} BTreeNodeSib;

int btree_node_delete_impl(
    BTreeNode* root, BTreeKey key, BTreeNode** new_root_ptr);

#endif