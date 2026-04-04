// BTREE

#ifndef __BTREE_CORE_H__
#define __BTREE_CORE_H__

#include "./delete.h"
#include "./insert.h"

struct BTreeNode;
typedef struct BTreeNode BTreeNode;

typedef struct BTree
{
    BTreeNode* root;
} BTree;

int btree_insert(BTree* tree, BTreeKey key);

int btree_delete(BTree* tree, BTreeKey key);

void btree_kill(BTree* tree);

#endif
