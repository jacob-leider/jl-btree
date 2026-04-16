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

int btree_insert(
    BTree* tree, BTreeKey* key, BTreeInsertionAlgorithm alg, char** err_msg);

int btree_delete(BTree* tree, BTreeKey* key);

void btree_kill(BTree* tree);

BTree* btree_init(size_t order);

#endif
