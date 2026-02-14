// Test utilities for BTree

#ifndef __BTREE_TEST_UTILS_H__
#define __BTREE_TEST_UTILS_H__

typedef struct BTreeNode BTreeNode;


void testutils_init();

int btree_node_is_valid_partial(BTreeNode* node, char** err_msg);

int btree_cmp(BTreeNode* a, BTreeNode* b);

int btree_check_subtree_sizes(BTreeNode* root);


// Pertaining to the btree

int btree_size(BTreeNode* root);

int btree_subtree_in_order_traverse(BTreeNode* root);


#endif
