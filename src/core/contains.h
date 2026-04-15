#ifndef __BTREE_CONTAINS_H__
#define __BTREE_CONTAINS_H__

#include "btree_node.h"

int btree_node_contains_key(BTreeNode* root, BTreeKey* key);

#endif