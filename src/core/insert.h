#ifndef __BTREE_INSERT_H__
#define __BTREE_INSERT_H__

#include "./btree_node.h"

typedef struct BTreeNode BTreeNode;

typedef enum BTreeInsertionAlgorithm
{
    TopdownLazy,
    TopdownNonLazy,
    BottomUp,
} BTreeInsertionAlgorithm;

int btree_node_insert_impl(BTreeNode* root,
    BTreeKey* key,
    BTreeNode** new_root_ptr,
    BTreeInsertionAlgorithm alg,
    char** err_msg);

#endif