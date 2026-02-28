#ifndef __BTREE_INSERT_H__
#define __BTREE_INSERT_H__

#include <stdbool.h>

#include "./btree_node.h"

typedef struct BTreeNode BTreeNode;

int btree_node_insert_impl(
    BTreeNode* root, const BTreeKey key, const BTreeNode** new_root_ptr);

#endif