#ifndef __BTREE_INSERT_H__
#define __BTREE_INSERT_H__

typedef struct BTreeNode BTreeNode;

int btree_node_insert_impl(
    BTreeNode* root, const int key, const BTreeNode** new_root_ptr);
  

#endif