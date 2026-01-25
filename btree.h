// BTREE

#ifndef __BTREE_CORE_H__
#define __BTREE_CORE_H__

struct BTreeNode;
typedef struct BTreeNode BTreeNode;

void btree_subtree_kill(BTreeNode* node);

int btree_node_insert_impl(BTreeNode* root, int val, BTreeNode** new_root_ptr);

int btree_node_delete_impl(BTreeNode* root, int val, BTreeNode** new_root_ptr);

#endif
