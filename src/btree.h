// BTREE

#ifndef __BTREE_CORE_H__
#define __BTREE_CORE_H__

#include "./delete.h"
#include "./insert.h"

struct BTreeNode;
typedef struct BTreeNode BTreeNode;

int btree_node_contains_key(BTreeNode* root, int key);

void btree_subtree_kill(BTreeNode* node);

int btree_node_insert_impl(
    BTreeNode* root, const BTreeKey key, const BTreeNode** new_root_ptr);

int btree_node_delete_impl(
    BTreeNode* root, const BTreeKey key, BTreeNode** new_root_ptr);

#endif
