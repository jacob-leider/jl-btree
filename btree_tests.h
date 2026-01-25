#ifndef __BTREE_TESTS_H__
#define __BTREE_TESTS_H__

// Test int
//
//    btree_node_insert_impl(BTreeNode* root, int key, BTreeNode** new_root_ptr)
//
// Let `leaf` be the leaf descendant of `root` that `key` can be inserted into

// 1. `key` is not in the tree.
// 2. `leaf` is full 
// 3. `leaf`'s parent is full
// 4. `leaf` has an ancestor that is not full

int TestBTreeNodeInsertImpl();

int TestBTreeNodeDeleteImpl();

#endif
