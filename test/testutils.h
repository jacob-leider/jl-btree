// Test utilities for BTree

#ifndef __BTREE_TEST_UTILS_H__
#define __BTREE_TEST_UTILS_H__

#include <stdbool.h>
#include <stdio.h>

typedef struct Stack Stack;

typedef struct BTreeNode BTreeNode;

typedef struct BTreeCmpSettings
{
    BTreeNode* a_root;
    BTreeNode* b_root;
    const char* a_name;
    const char* b_name;
    char* log_file_path;
} BTreeCmpSettings;

void testutils_init();

int btree_node_is_valid_partial(BTreeNode* node, char** err_msg);

bool btree_cmp(BTreeCmpSettings* settings);

int btree_check_subtree_sizes(BTreeNode* root);

// Pertaining to the btree

int btree_size(BTreeNode* root);

int btree_subtree_in_order_traverse(BTreeNode* root);

#endif
